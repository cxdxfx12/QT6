#include "Customer.h"

Customer::Customer(const QString &name) : m_name(name) {
    m_rule.customerName = name;
}

QString Customer::name() const { return m_name; }
void Customer::setName(const QString &name) { m_name = name; }

CustomerRule Customer::rule() const { return m_rule; }
void Customer::setRule(const CustomerRule &rule) { m_rule = rule; }

bool Customer::hasCustomPrice() const { return !m_rule.useDefaultPrice; }
