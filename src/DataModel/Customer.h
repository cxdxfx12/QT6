#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <QString>
#include <QList>
#include "PriceTable.h"

class Customer {
public:
    Customer() = default;
    Customer(const QString &name);

    QString name() const;
    void setName(const QString &name);

    CustomerRule rule() const;
    void setRule(const CustomerRule &rule);

    bool hasCustomPrice() const;

private:
    QString m_name;
    CustomerRule m_rule;
};

#endif // CUSTOMER_H
