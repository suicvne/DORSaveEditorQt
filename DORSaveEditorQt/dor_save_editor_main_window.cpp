#include "dor_save_editor_main_window.h"
#include "ui_dor_save_editor_main_window.h"

#include <DOR.h>
#include <PSU.h>

#include <QFileDialog.h>
#include <QHeaderView>
#include <QMessageBox>
#include <QTreeView>

namespace
{
void ConfigureTreeViewColumnSizing(QTreeView* TreeView, int NameColumn, int ValueColumn)
{
    QHeaderView* Header = TreeView->header();

    Header->setStretchLastSection(false);
    Header->setMinimumSectionSize(80);
    Header->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    Header->setSectionResizeMode(ValueColumn, QHeaderView::ResizeToContents);
    Header->setResizeContentsPrecision(100);
}

void ExpandTreeViewRoot(QTreeView* TreeView)
{
    TreeView->expandToDepth(0);
}
}

DORSaveTreeViewerMainWindow::DORSaveTreeViewerMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::DORSaveTreeViewerMainWindow),
      ChestModel(this),
      DecksModel(this)
{
    ui->setupUi(this);

    ui->chestTreeView->setModel(&ChestModel);
    ui->decksTreeView->setModel(&DecksModel);
    ConfigureTreeViewColumnSizing(ui->chestTreeView, DORChestModel::NameColumn, DORChestModel::ValueColumn);
    ConfigureTreeViewColumnSizing(ui->decksTreeView, DORDecksModel::NameColumn, DORDecksModel::ValueColumn);
    ExpandTreeViewRoot(ui->chestTreeView);
    ExpandTreeViewRoot(ui->decksTreeView);

    SyncWindowTitle();
}

DORSaveTreeViewerMainWindow::~DORSaveTreeViewerMainWindow()
{
    delete ui;
}

void DORSaveTreeViewerMainWindow::on_actionOpen_Save_triggered()
{
    QFileDialog qfd(this);
    qfd.setFilter(QDir::Filter::Files);
    qfd.setNameFilter("*.psu");

    qfd.show();
    int Result = qfd.exec();

    if(Result != QDialog::Accepted) { return; }

    // grab first opened file.
    const auto SelectedFiles = qfd.selectedFiles();
    auto FirstFile = SelectedFiles.first();

    OpenSaveFile(FirstFile);
    SyncWindowTitle();
}

void DORSaveTreeViewerMainWindow::SyncWindowTitle()
{
    if(Ctx.pSave == nullptr)
    {
        setWindowTitle("DOR Save Editor");
        return;
    }

    // format -- show player name in the title bar.
    QString Fmt("DOR Save Editor - %1");
    char PlayerName[16] = { 0 };

    DORSave_GetPlayerName(Ctx.pSave, PlayerName, sizeof(PlayerName));

    setWindowTitle(Fmt.arg(PlayerName));
}

void DORSaveTreeViewerMainWindow::ShowError(QString Title, QString Message)
{
    QMessageBox qmb(this);
    qmb.setText(Message);

    qmb.exec();
}

void DORSaveTreeViewerMainWindow::OpenSaveFile(QString InPath)
{
    PSUStatus LastPSUStatus;
    DORStatus LastDORStatus;

    // close previous, if appropriate.
    if(Ctx.pSave != nullptr)
    {
        ChestModel.SetSave(nullptr);
        DecksModel.SetSave(nullptr);

        DORSave_Destroy(Ctx.pSave);
        PSUArchive_Destroy(Ctx.pArchive);

        Ctx.pSave = nullptr;
        Ctx.pArchive = nullptr;
    }

    // Open PSU archive.
    auto AsUtf8 = InPath.toUtf8();
    LastPSUStatus = PSUArchive_CreateFromFile(AsUtf8.constData(), &Ctx.pArchive);
    if(LastPSUStatus != PSUStatusOk)
    {
        ShowError("Error opening save", QString("Error: %1").arg(PSUStatus_ToString(LastPSUStatus)));
        return;
    }

    // open DOR:
    DORSave_CreateFromPSUArchive(Ctx.pArchive, &Ctx.pSave);

    // set path:
    Ctx.Path = InPath;
    setWindowFilePath(Ctx.Path);

    // bump update the model:
    ChestModel.SetSave(Ctx.pSave);
    DecksModel.SetSave(Ctx.pSave);
    ExpandTreeViewRoot(ui->chestTreeView);
    ExpandTreeViewRoot(ui->decksTreeView);

}
