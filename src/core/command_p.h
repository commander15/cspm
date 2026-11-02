#ifndef COMMAND_P_H
#define COMMAND_P_H

#include "command.h"

#include <QCommandLineParser>
#include <QStringList>

class DefaultCommand : public Command
{
public:
    ~DefaultCommand();

    void init(QCommandLineParser &parser) override
    {
        parser.addVersionOption();
    }

    int run(QCommandLineParser &parser) override
    {
        static const QStringList commands = { "update", "install", "upgrade", "remove", "build" };
        console.out << "available commands: " << commands.join(", ") << Qt::endl;
        return 0;
    }

private:
    QList<class DefaultCommandProvider *> m_destroyers;

    friend class DefaultCommandProvider;
};

class DefaultCommandProvider
{
public:
    DefaultCommandProvider() : m_command(nullptr) { }
    ~DefaultCommandProvider() { if (m_command != nullptr) { m_command->m_destroyers.removeOne(this); delete m_command; } }

    DefaultCommand *get()
    {
        if (m_command == nullptr) recreate();
        return m_command;
    }

    void recreate()
    {
        if (m_command == nullptr) {
            m_command = new DefaultCommand();
            m_command->m_destroyers.append(this);
        }
    }

private:
    DefaultCommand *m_command;

    friend class DefaultCommand;
};


inline DefaultCommand::~DefaultCommand()
{
    for (auto *d : std::as_const(m_destroyers))
        d->m_command = nullptr;
}

#endif // COMMAND_P_H
