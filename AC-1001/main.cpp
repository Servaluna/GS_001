#include "app/appcontroller.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    AppController controller;
    const int result = controller.start();
    if (result != 0) {
        return result;
    }

    return app.exec();
}
