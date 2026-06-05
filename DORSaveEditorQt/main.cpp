#include "dor_save_editor_main_window.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    DORSaveTreeViewerMainWindow w;
    w.show();

    return QCoreApplication::exec();
}
