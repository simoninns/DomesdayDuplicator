#include "domesdayduplicator.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    domesdayDuplicator w;
    w.show();

    return a.exec();
}
