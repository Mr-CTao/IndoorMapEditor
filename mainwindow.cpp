﻿#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "./gui/documentview.h"
#include "./gui/propertyview.h"
#include "./gui/propviewfuncarea.h"
#include "./gui/propviewbuilding.h"
#include "./gui/propviewfloor.h"
#include "./gui/scenemodel.h"
#include "./core/building.h"
#include "./core/scene.h"
#include "./io/iomanager.h"
#include "./tool/toolmanager.h"
#include "./tool/polygontool.h"
#include "./tool/selecttool.h"
#include "./tool/pubpointtool.h"
#include "./tool/mergetool.h"
#include "./tool/splittool.h"
#include "./tool/scaletool.h"
#include <QTimer>
#include <QCloseEvent>
#include <QSettings>
#include <QFileDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QTreeView>
#ifndef QT_NO_PRINTER
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrintPreviewDialog>
#endif

#pragma execution_character_set("utf-8")

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_maxRecentFiles(5),
    m_lastFilePath("."),
    m_printer(NULL),
    m_propertyView(NULL),
    m_timer(NULL),
    m_searchResultIter(m_searchResults)
{
    ui->setupUi(this);

    m_sceneTreeView = new QTreeView(ui->dockTreeWidget);
    //ui->dockTreeWidget->setWidget(m_sceneTreeView);
    ui->verticalLayout->addWidget(m_sceneTreeView);

    QActionGroup * toolActionGroup = new QActionGroup(this);
    toolActionGroup->addAction(ui->actionSelectTool);
    toolActionGroup->addAction(ui->actionPolygonTool);
    toolActionGroup->addAction(ui->actionPubPointTool);
    toolActionGroup->addAction(ui->actionMergeTool);
    toolActionGroup->addAction(ui->actionSplitTool);
    toolActionGroup->addAction(ui->actionScaleTool);

    //recent file actions
    QMenu *recentFileMenu = new QMenu(this);
    m_recentFileActions.resize(m_maxRecentFiles);
    for(int i = 0; i < m_recentFileActions.size(); i++){
        m_recentFileActions[i]= new QAction(this);
        m_recentFileActions[i]->setVisible(false);
        connect(m_recentFileActions[i], SIGNAL(triggered()), this, SLOT(openRecentFile()));
        recentFileMenu->addAction(m_recentFileActions[i]);
    }
    ui->actionRecent->setMenu(recentFileMenu);
    //menus action
    connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(openFile()));
    connect(ui->actionNew, SIGNAL(triggered()), this, SLOT(newFile()));
    connect(ui->actionSave, SIGNAL(triggered()), this, SLOT(saveFile()));
    connect(ui->actionSaveAs, SIGNAL(triggered()), this, SLOT(saveAsFile()));
    connect(ui->actionClose, SIGNAL(triggered()), this, SLOT(closeFile()));
    connect(ui->actionPrint, SIGNAL(triggered()), this, SLOT(printFile()));
    connect(ui->actionPrintCurrent, SIGNAL(triggered()), this, SLOT(printCurrent()));
    connect(ui->actionDelete, SIGNAL(triggered()), this, SLOT(deleteEntity()));
    connect(ui->actionFont, SIGNAL(triggered()), this, SLOT(setGraphicsViewFont()));

    //tools action
    connect(ui->actionPolygonTool, SIGNAL(triggered()), this, SLOT(setPolygonTool()));
    connect(ui->actionSelectTool, SIGNAL(triggered()), this, SLOT(setSelectTool()));
    connect(ui->actionPubPointTool, SIGNAL(triggered()), this, SLOT(setPubPointTool()));
    connect(ui->actionMergeTool, SIGNAL(triggered()), this, SLOT(setMergeTool()));
    connect(ui->actionSplitTool, SIGNAL(triggered()), this, SLOT(setSplitTool()));
    connect(ui->actionScaleTool, SIGNAL(triggered()), this, SLOT(setScaleTool()));

    addDocument(new DocumentView());
    setCurrentFile("");
    rebuildTreeView();

    ToolManager::instance()->setTool(new SelectTool(currentDocument()));

    connect(m_sceneTreeView, SIGNAL(clicked(QModelIndex)), m_docView, SLOT(updateSelection(QModelIndex)));
    connect(m_docView, SIGNAL(selectionChanged(MapEntity*)), this, SLOT(updatePropertyView(MapEntity*)));
    connect(m_docView->scene(), SIGNAL(buildingChanged()), this, SLOT(rebuildTreeView()));
    connect(ui->actionShowShopText, SIGNAL(toggled(bool)), m_docView, SLOT(showShopText(bool)));
    connect(ui->actionShowPointText, SIGNAL(toggled(bool)), m_docView, SLOT(showPointText(bool)));
    connect(ui->actionShowDir, SIGNAL(toggled(bool)), m_docView, SLOT(showDirection(bool)));
    connect(ui->actionZoomOut, SIGNAL(triggered()), m_docView, SLOT(zoomOut()));
    connect(ui->actionZoomIn, SIGNAL(triggered()), m_docView, SLOT(zoomIn()));
    connect(ui->actionResetZoom, SIGNAL(triggered()), m_docView, SLOT(fitView()));
    connect(ui->searchButton, SIGNAL(clicked()), this, SLOT(onSearch()) );
    ui->preResultButton->setVisible(false);
    ui->nextResultButton->setVisible(false);
    connect(ui->preResultButton, SIGNAL(clicked()), this, SLOT(selectPreviousResult()) );
    connect(ui->nextResultButton, SIGNAL(clicked()), this, SLOT(selectNextResult()) );

    m_docView->scene()->setFont(QFont(tr("微软雅黑"), 26));

    //autosave
    m_autoSaveTime = 300000; //autosave every 5min
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(autoSave()) );
    m_timer->start(m_autoSaveTime);


    readSettings();
    QSettings settings("Wolfwind", "IndoorMapEditor");
    settings.setValue("normallyClosed", false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event){
    if(okToContinue()){
        writeSettings();
        event->accept();
    }else{
        event->ignore();
    }
}

DocumentView *MainWindow::currentDocument() const
{
    return m_docView;
}

void MainWindow::openFile()
{
    if(okToContinue()){
        //TODO: vector graphic file support
        QString fileName = QFileDialog::getOpenFileName(this,
                                                        tr("打开文件"), m_lastFilePath,
                                                        tr("全部文件 (*.json *.jpg *.jpeg *.png *.bmp *.gif)\n"
                                                            "Json文件 (*.json)\n"
                                                           "图像文件 (*.jpg *.jpeg *.png *.bmp *.gif)"));
        if(!fileName.isEmpty())
            openDocument(fileName);
    }
}

void MainWindow::openRecentFile(){
    if(okToContinue()){
        QAction *action = qobject_cast<QAction *>(sender());
        if(action){
            openDocument(action->data().toString());
        }
    }
}

void MainWindow::addDocument(DocumentView *doc) {
    //TODO: connect the slots
    m_docView = doc;
    this->setCentralWidget(doc);
}

void MainWindow::openDocument(const QString &fileName){
    m_lastFilePath = QFileInfo(fileName).absoluteFilePath();//save the last path
    currentDocument()->clear();
    if(IOManager::loadFile(fileName, currentDocument()))
    {
         rebuildTreeView(); //rebuild the treeView
        statusBar()->showMessage(tr("文件载入成功"), 2000);
        if(QFileInfo(fileName).suffix() == "json"){
            setCurrentFile(fileName);
            autoSave();
        }
        currentDocument()->scene()->showDefaultFloor();
        currentDocument()->fitView();

    }else{
        QMessageBox::warning(this,
                            tr("Parse error"),
                            tr("文件载入失败\n%1").arg(fileName));
        return;
    }
}

bool MainWindow::saveDocument(const QString &fileName){
    if(IOManager::saveFile(fileName, currentDocument())){
        statusBar()->showMessage(tr("文件保存成功"), 2000);
        setCurrentFile(fileName);
        return true;
    }else{
        statusBar()->showMessage(tr("文件保存失败"), 2000);
        return false;
    }
}

void MainWindow::newFile() {
    if(okToContinue()){
        currentDocument()->clear();
        setCurrentFile("");

        rebuildTreeView();
    }
}

bool MainWindow::saveFile() {
    if(m_curFile.isEmpty()){
        return saveAsFile();
    }else{
        return saveDocument(m_curFile);
    }
}

bool MainWindow::saveAsFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Files"),".",tr("Indoor map files(*.json)"));
    if(fileName.isEmpty()){
        return false;
    }

    return saveDocument(fileName);
}

void MainWindow::autoSave(){
    QString tmpDir = QDir::tempPath();
    IOManager::saveFile(tmpDir + "/autoSaveFile.json", currentDocument());
}

void MainWindow::closeFile()
{

}

void MainWindow::exportFile()
{

}

void MainWindow::printFile()
{
    if(m_printer == NULL)
        m_printer = new QPrinter(QPrinter::HighResolution);

    if(!m_printer->isValid()){
        QMessageBox::warning(this, tr("Error"),tr("No printer found"),QMessageBox::Ok);
        return;
    }

    QPrintPreviewDialog preview(m_printer, this);
    //m_printer->setOutputFormat(QPrinter::NativeFormat);

    connect(&preview, SIGNAL(paintRequested(QPrinter*)),
             currentDocument(), SLOT(printScene(QPrinter*)));

    preview.exec();

//    QPrintDialog printDialog(m_printer, this);
//    if(printDialog.exec() == QDialog::Accepted){
//        QPainter painter(m_printer);
//        currentDocument()->printScene(&painter);
//        statusBar()->showMessage(tr("Printed %1").arg(windowFilePath()), 2000);
//    }
}

void MainWindow::printCurrent(){
    if(m_printer == NULL)
        m_printer = new QPrinter(QPrinter::HighResolution);

    if(!m_printer->isValid()){
        QMessageBox::warning(this, tr("Error"),tr("No printer found"),QMessageBox::Ok);
        return;
    }

    QPrintPreviewDialog preview(m_printer, this);

    connect(&preview, SIGNAL(paintRequested(QPrinter*)),
             currentDocument(), SLOT(printCurrentView(QPrinter*)));

    preview.exec();
}

void MainWindow::deleteEntity(){
    currentDocument()->scene()->deleteSelected();
    rebuildTreeView();
}

void MainWindow::setCurrentFile(const QString & fileName){
    m_curFile = fileName;
    currentDocument()->setModified(false);

    QString shownName = tr("Untitle");
    if(! m_curFile.isEmpty()){
        shownName = QFileInfo(fileName).fileName();
        m_recentFiles.removeAll(m_curFile);
        m_recentFiles.prepend(m_curFile);
        updateRecentFileActions();
    }
    setWindowTitle(tr("%1[*] - %2").arg(shownName).arg(tr("IndoorMap Editor")));
}

void MainWindow::readSettings(){
    QSettings settings("Wolfwind", "IndoorMapEditor");

    bool normallyClosed = settings.value("normallyClosed", false).toBool();
    if(!normallyClosed){
        QString path = QDir::tempPath();
        QFileInfo fileInfo(path + "/autoSaveFile.json");
        if(fileInfo.exists()){
            int r = QMessageBox::warning(this, tr("Warning"),
                                         tr("发现意外关闭时的缓存文件，是否恢复？"),
                                         QMessageBox::Yes | QMessageBox::No );
            if(r == QMessageBox::Yes){
                //open the autosave file
                openDocument(fileInfo.absoluteFilePath());
                m_curFile.clear();
            }
        }
    }

    m_recentFiles = settings.value("recentFiles").toStringList();
    updateRecentFileActions();
}

void MainWindow::writeSettings(){
    QSettings settings("Wolfwind", "IndoorMapEditor");
    settings.setValue("recentFiles", m_recentFiles);
    settings.setValue("normallyClosed", true);
}

void MainWindow::updateRecentFileActions(){
    QMutableStringListIterator i(m_recentFiles);
    while(i.hasNext()){
        if(!QFile::exists(i.next()))
            i.remove();
    }
    for(int j = 0; j < m_recentFileActions.size(); j++){
        if(j < m_recentFiles.count()){
            QString text = tr("&%1 %2").arg(j+1).arg( QFileInfo(m_recentFiles[j]).fileName());
            m_recentFileActions[j]->setText(text);
            m_recentFileActions[j]->setData(m_recentFiles[j]);
            m_recentFileActions[j]->setVisible(true);
        }else{
            m_recentFileActions[j]->setVisible(false);
        }
    }
}

bool MainWindow::okToContinue(){
    if(currentDocument()->isModified()){
        int r = QMessageBox::warning(this, tr("Warning"),
                                     tr("the file has been modifed\n"
                                     "do you want to save? "),
                                     QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if(r == QMessageBox::Yes){
            return saveFile();
        }else if(r == QMessageBox::Cancel){
            return false;
        }
    }
    return true;
}

void MainWindow::rebuildTreeView(){
    SceneModel *model = new SceneModel(m_docView->scene()->root());
    m_sceneTreeView->setModel(model);
    m_sceneTreeView->expandToDepth(0);
}

void MainWindow::updatePropertyView(MapEntity *mapEntity) {
    if(mapEntity == NULL && m_propertyView!=NULL){
        delete m_propertyView;
        m_propertyView = NULL;
        return;
    }
    QString className = mapEntity->metaObject()->className();

    if(m_propertyView== NULL || !m_propertyView->match(mapEntity)){
        if(m_propertyView != NULL)
            delete m_propertyView;
        //ugly codes. should be replaced by a factory class later.
        if(className == "FuncArea"){
            m_propertyView = new PropViewFuncArea(ui->dockPropertyWidget);
        }else if(className == "Building"){
            m_propertyView = new PropViewBuilding(ui->dockPropertyWidget);
        }else if(className == "Floor"){
            m_propertyView = new PropViewFloor(ui->dockPropertyWidget);
        }else{
            m_propertyView = new PropertyView(ui->dockPropertyWidget);
        }
        ui->dockPropertyWidget->setWidget(m_propertyView);
    }
    m_propertyView->setMapEntity(mapEntity);
}

void MainWindow::setPolygonTool(){
    AbstractTool *tool = new PolygonTool(currentDocument());
    ToolManager::instance()->setTool(tool);
    currentDocument()->setSelectable(false);
}

void MainWindow::setSelectTool(){
    AbstractTool *tool = new SelectTool(currentDocument());
    ToolManager::instance()->setTool(tool);
    currentDocument()->setSelectable(true);
}

void MainWindow::setPubPointTool(){
    AbstractTool *tool = new PubPointTool(currentDocument());
    ToolManager::instance()->setTool(tool);
    currentDocument()->setSelectable(false);
}

void MainWindow::setMergeTool(){
    AbstractTool *tool = new MergeTool(currentDocument());
    ToolManager::instance()->setTool(tool);
    currentDocument()->setSelectable(true);
}

void MainWindow::setSplitTool(){
    AbstractTool *tool = new SplitTool(currentDocument());
    ToolManager::instance()->setTool(tool);
    currentDocument()->setSelectable(false);
}

void MainWindow::setScaleTool(){
    AbstractTool *tool = new ScaleTool(currentDocument());
    ToolManager::instance()->setTool(tool);
    currentDocument()->setSelectable(false);
}

void MainWindow::setGraphicsViewFont(){
    bool ok;
    //QFontDialog::setCurrentFont(QApplication::font("DocumentView"));
    QGraphicsScene *scene = currentDocument()->scene();
    QFont font = QFontDialog::getFont(&ok, scene->font(), this );
    if ( ok ) {
          scene->setFont(font);
    }
}

void MainWindow::onSearch(){
    QString searchText = ui->searchEdit->text();
    m_searchResults = currentDocument()->scene()->findMapEntity(searchText);
    m_searchResultIter = QListIterator<MapEntity*>(m_searchResults);
    if(m_searchResults.isEmpty()){
        QMessageBox::warning(this, tr("搜索结果"),(QString("未找到 ")+searchText),QMessageBox::Ok);
        return;
    }else if(m_searchResults.size() > 1){
        ui->preResultButton->setVisible(true);
        ui->nextResultButton->setVisible(true);
        ui->preResultButton->setEnabled(false);
        ui->nextResultButton->setEnabled(true);
    }else{
        ui->preResultButton->setVisible(false);
        ui->nextResultButton->setVisible(false);
    }
    currentDocument()->scene()->selectMapEntity((m_searchResultIter.next()));

}

void MainWindow::selectPreviousResult(){

    currentDocument()->scene()->selectMapEntity((m_searchResultIter.previous()));
    ui->nextResultButton->setEnabled(true);
    if(!m_searchResultIter.hasPrevious()){
        ui->preResultButton->setEnabled(false);
        m_searchResultIter.next();
    }
}

void MainWindow::selectNextResult(){
    currentDocument()->scene()->selectMapEntity((m_searchResultIter.next()));
    ui->preResultButton->setEnabled(true);
    if(!m_searchResultIter.hasNext() ){
        ui->nextResultButton->setEnabled(false);
        m_searchResultIter.previous();
    }

}
