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
        Header->setSectionResizeMode(NameColumn, QHeaderView::Interactive);
        Header->setSectionResizeMode(ValueColumn, QHeaderView::Interactive);
        Header->setResizeContentsPrecision(100);
    }

    void AutoSizeTreeViewColumns(QTreeView* TreeView)
    {
        for(int Column = 0; Column < TreeView->model()->columnCount(); ++Column)
        {
            TreeView->resizeColumnToContents(Column);
        }
    }

void ExpandTreeViewToDepth(QTreeView* TreeView, int Depth)
{
    TreeView->expandToDepth(Depth);
}
}

DORSaveTreeViewerMainWindow::DORSaveTreeViewerMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::DORSaveTreeViewerMainWindow),
      InfoTabModel(this),
      ChestModel(this),
      DecksModel(this)
{
    ui->setupUi(this);

    ui->infoTreeView->setModel(&InfoTabModel);
    ui->chestTreeView->setModel(&ChestModel);
    ui->decksTreeView->setModel(&DecksModel);
    ConfigureTreeViewColumnSizing(ui->infoTreeView, DORInfoTabModel::NameColumn, DORInfoTabModel::ValueColumn);
    ConfigureTreeViewColumnSizing(ui->chestTreeView, DORChestModel::NameColumn, DORChestModel::ValueColumn);
    ConfigureTreeViewColumnSizing(ui->decksTreeView, DORDecksModel::NameColumn, DORDecksModel::ValueColumn);
    ExpandTreeViewToDepth(ui->infoTreeView, 1);
    ExpandTreeViewToDepth(ui->chestTreeView, 0);
    ExpandTreeViewToDepth(ui->decksTreeView, 0);
    AutoSizeTreeViewColumns(ui->infoTreeView);
    AutoSizeTreeViewColumns(ui->chestTreeView);
    AutoSizeTreeViewColumns(ui->decksTreeView);

    connect(&InfoTabModel, &QAbstractItemModel::dataChanged, this, [this]() {
        setWindowModified(true);
        SyncWindowTitle();
    });

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
        InfoTabModel.SetContext(nullptr, nullptr, QString());
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
    LastDORStatus = DORSave_CreateFromPSUArchive(Ctx.pArchive, &Ctx.pSave);
    if(LastDORStatus != DORStatusOk)
    {
        ShowError("Error opening save", QString("Error: %1").arg(DORStatus_ToString(LastDORStatus)));
        PSUArchive_Destroy(Ctx.pArchive);
        Ctx.pArchive = nullptr;
        return;
    }

    // set path:
    Ctx.Path = InPath;
    setWindowFilePath(Ctx.Path);

    // bump update the model:
    InfoTabModel.SetContext(Ctx.pSave, Ctx.pArchive, Ctx.Path);
    ChestModel.SetSave(Ctx.pSave);
    DecksModel.SetSave(Ctx.pSave);
    ExpandTreeViewToDepth(ui->infoTreeView, 1);
    ExpandTreeViewToDepth(ui->chestTreeView, 0);
    ExpandTreeViewToDepth(ui->decksTreeView, 0);
    AutoSizeTreeViewColumns(ui->infoTreeView);
    AutoSizeTreeViewColumns(ui->chestTreeView);
    AutoSizeTreeViewColumns(ui->decksTreeView);

}
