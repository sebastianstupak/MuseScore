/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 */
#ifndef MU_NOTATION_CORRECTIONTAXONOMY_H
#define MU_NOTATION_CORRECTIONTAXONOMY_H

#include <QList>
#include <QString>

class QJsonObject;

namespace mu::notation {
struct TaxonEntry {
    QString id;
    QString display;
    QString group;
    QString kind;
    QString applyKind;
    QString applyParam;
    QString homusLabel;
};

const QList<TaxonEntry>& taxonomy();
QJsonObject taxonLabelObject(const TaxonEntry& e);
const TaxonEntry* taxonByHomusLabel(const QString& homusLabel);
}

#endif // MU_NOTATION_CORRECTIONTAXONOMY_H
