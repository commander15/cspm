#ifndef COMMAND_H
#define COMMAND_H

#include <QTextStream>
#include <QEventLoop>

class QCommandLineParser;

class ConsoleStreams {
public:
    QTextStream &out;
    QTextStream &err;
    QTextStream &in;

    static ConsoleStreams null() {
        static QByteArray dummyBuffer;
        static QTextStream nullOut(&dummyBuffer);
        static QTextStream nullErr(&dummyBuffer);
        static QTextStream nullIn(&dummyBuffer);

        dummyBuffer.clear();
        return ConsoleStreams{ nullOut, nullErr, nullIn };
    }
};

class Command
{
public:
    Command();
    virtual ~Command();

    virtual void init(QCommandLineParser &parser);
    virtual int run(QCommandLineParser &parser) = 0;

    template<typename CMD>
    static Command *create() { return new CMD(); }
    static Command *defaultCommand();

    ConsoleStreams console;

protected:
    void exit(bool exitCode = 0);
    int exec();

private:
    QTextStream out;
    QTextStream err;
    QTextStream in;

    QEventLoop m_loop;
};

#endif // COMMAND_H
