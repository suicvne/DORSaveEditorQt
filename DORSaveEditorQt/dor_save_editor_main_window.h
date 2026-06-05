#ifndef DOR_SAVE_EDITOR_MAIN_WINDOW_H
#define DOR_SAVE_EDITOR_MAIN_WINDOW_H

#include <QMainWindow>

#include "dor_chest_model.h"
#include "dor_decks_model.h"

struct PSUArchive;
struct DORSave;

struct AppContext {
    PSUArchive* pArchive;
    DORSave*    pSave;

    QString     Path;
};

QT_BEGIN_NAMESPACE
namespace Ui {
class DORSaveTreeViewerMainWindow;
}
QT_END_NAMESPACE

class DORSaveTreeViewerMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit DORSaveTreeViewerMainWindow(QWidget *parent = nullptr);
    ~DORSaveTreeViewerMainWindow() override;

    void SyncWindowTitle();
    void OpenSaveFile(QString InPath);
    void ShowError(QString Title, QString Message);

private slots:
    void on_actionOpen_Save_triggered();

private:
    Ui::DORSaveTreeViewerMainWindow *ui;
    AppContext Ctx = {};
    DORChestModel ChestModel;
    DORDecksModel DecksModel;
};
#endif // DOR_SAVE_EDITOR_MAIN_WINDOW_H
