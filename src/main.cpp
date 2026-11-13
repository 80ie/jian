#include <QApplication>
#include <QCommandLineParser>

#include <cstdio>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    std::fputs(qPrintable(QCommandLineParser().helpText()), stderr);
    return 2;
}
