#ifndef MODEL_H
#define MODEL_H

#include <QEloquent/model.h>
#include <QEloquent/modelhelpers.h>

#include <QDateTime>

class Model : public QEloquent::Model
{
    Q_GADGET
    Q_PROPERTY(int id MEMBER id FINAL)
    Q_PROPERTY(QDateTime createdAt MEMBER createdAt FINAL)
    Q_PROPERTY(QDateTime updatedAt MEMBER updatedAt FINAL)

public:
    int id = 0;
    QDateTime createdAt;
    QDateTime updatedAt;

protected:
    template<typename T, std::enable_if<std::is_base_of<Model, T>::value>::type* = nullptr>
    Model(T *self) : QEloquent::Model(self) {}
};

using QEloquent::ModelHelpers;
using QEloquent::Relation;

#endif // MODEL_H
