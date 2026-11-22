#include "main_window.hpp"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName("RedesChat");
    QApplication::setOrganizationName("Redes 2");

    MainWindow window;
    window.show();

    return application.exec();
}
