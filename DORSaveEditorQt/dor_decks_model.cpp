#include "dor_decks_model.h"

#include <QString>

DORDecksModel::DORDecksModel(QObject *parent)
    : QAbstractItemModel{parent}
{}

void DORDecksModel::SetSave(const DORSave* InSave)
{
    beginResetModel();
    Save = InSave;
    endResetModel();
}

const DORSave* DORDecksModel::GetSave() const
{
    return Save;
}

QModelIndex DORDecksModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) { return QModelIndex(); }

    if(!parent.isValid())
    {
        if(row != 0) { return QModelIndex(); }
        return createIndex(row, column, MakeNodeId(DecksNode, 0));
    }

    switch(NodeType(parent))
    {
        case DecksNode:
            if(Save == nullptr || row < 0 || row >= 3) { return QModelIndex(); }
            return createIndex(row, column, MakeNodeId(DeckNode, static_cast<uint>(row)));

        case DeckNode:
        {
            const uint DeckIndex = NodeValue(parent);
            if(Save == nullptr || DeckIndex >= 3 || row < 0 || row > static_cast<int>(DORDeckCardCount)) { return QModelIndex(); }

            if(row == 0) { return createIndex(row, column, MakeNodeId(LeaderNode, DeckIndex)); }

            const uint CardIndex = static_cast<uint>(row - 1);
            return createIndex(row, column, MakeNodeId(CardNode, DeckIndex * DORDeckCardCount + CardIndex));
        }

        case LeaderNode:
        case CardNode:
        default:
            return QModelIndex();
    }
}

QModelIndex DORDecksModel::parent(const QModelIndex& child) const
{
    if(!child.isValid()) { return QModelIndex(); }

    switch(NodeType(child))
    {
        case DecksNode:
            return QModelIndex();

        case DeckNode:
            return createIndex(0, 0, MakeNodeId(DecksNode, 0));

        case LeaderNode:
        {
            const uint DeckIndex = NodeValue(child);
            return createIndex(static_cast<int>(DeckIndex), 0, MakeNodeId(DeckNode, DeckIndex));
        }

        case CardNode:
        {
            const uint CardValue = NodeValue(child);
            const uint DeckIndex = CardValue / DORDeckCardCount;
            return createIndex(static_cast<int>(DeckIndex), 0, MakeNodeId(DeckNode, DeckIndex));
        }

        default:
            return QModelIndex();
    }
}

int DORDecksModel::rowCount(const QModelIndex& parent) const
{
    if(parent.column() > 0) { return 0; }

    if(!parent.isValid()) { return 1; }

    switch(NodeType(parent))
    {
        case DecksNode:
            return Save == nullptr ? 0 : 3;

        case DeckNode:
            return Save == nullptr ? 0 : static_cast<int>(DORDeckCardCount + 1u);

        case LeaderNode:
        case CardNode:
        default:
            return 0;
    }
}

int DORDecksModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant DORDecksModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || role != Qt::DisplayRole) { return QVariant(); }

    switch(NodeType(index))
    {
        case DecksNode:
            if(index.column() == NameColumn) { return QStringLiteral("Decks"); }
            if(index.column() == ValueColumn) { return Save == nullptr ? QStringLiteral("No save loaded") : QStringLiteral("3 decks"); }
            return QVariant();

        case DeckNode:
            return DeckData(NodeValue(index), index.column());

        case LeaderNode:
            return LeaderData(NodeValue(index), index.column());

        case CardNode:
            return CardData(NodeValue(index), index.column());

        default:
            return QVariant();
    }
}

QVariant DORDecksModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole) { return QVariant(); }

    switch(section)
    {
        case NameColumn:
            return QStringLiteral("Entry");

        case ValueColumn:
            return QStringLiteral("Value");

        default:
            return QVariant();
    }
}

quintptr DORDecksModel::MakeNodeId(ModelNodeType Type, uint Value)
{
    return (static_cast<quintptr>(Value) << 3) | static_cast<quintptr>(Type);
}

DORDecksModel::ModelNodeType DORDecksModel::NodeType(const QModelIndex& Index)
{
    return Index.isValid() ? static_cast<ModelNodeType>(Index.internalId() & 0x7u) : InvalidNode;
}

uint DORDecksModel::NodeValue(const QModelIndex& Index)
{
    return static_cast<uint>(Index.internalId() >> 3);
}

QVariant DORDecksModel::DeckData(uint DeckIndex, int Column) const
{
    if(Save == nullptr || DeckIndex >= 3) { return QVariant(); }

    DORDeckInfo Info = {};
    if(DORSave_GetDeckInfo(Save, static_cast<DORDeckID>(DeckIndex), &Info) != DORStatusOk) { return QVariant(); }

    switch(Column)
    {
        case NameColumn:
            return DeckName(DeckIndex);

        case ValueColumn:
            return QStringLiteral("Deck cost %1").arg(Info.StoredDeckCost);

        default:
            return QVariant();
    }
}

QVariant DORDecksModel::LeaderData(uint DeckIndex, int Column) const
{
    if(Save == nullptr || DeckIndex >= 3) { return QVariant(); }

    DORDeckInfo Info = {};
    if(DORSave_GetDeckInfo(Save, static_cast<DORDeckID>(DeckIndex), &Info) != DORStatusOk) { return QVariant(); }

    switch(Column)
    {
        case NameColumn:
            return QStringLiteral("Leader");

        case ValueColumn:
            return FormatCard(Info.LeaderCardId);

        default:
            return QVariant();
    }
}

QVariant DORDecksModel::CardData(uint CardValue, int Column) const
{
    if(Save == nullptr) { return QVariant(); }

    const uint DeckIndex = CardValue / DORDeckCardCount;
    const uint CardIndex = CardValue % DORDeckCardCount;
    if(DeckIndex >= 3 || CardIndex >= DORDeckCardCount) { return QVariant(); }

    DORDeckInfo Info = {};
    if(DORSave_GetDeckInfo(Save, static_cast<DORDeckID>(DeckIndex), &Info) != DORStatusOk) { return QVariant(); }

    switch(Column)
    {
        case NameColumn:
            return QStringLiteral("Card %1").arg(CardIndex + 1u);

        case ValueColumn:
            return FormatCard(Info.Cards[CardIndex]);

        default:
            return QVariant();
    }
}

QString DORDecksModel::DeckName(uint DeckIndex)
{
    return QStringLiteral("Deck %1").arg(QChar(static_cast<char>('A' + DeckIndex)));
}

QString DORDecksModel::FormatCard(uint16_t CardId)
{
    if(CardId == DOREmptyCardId) { return QStringLiteral("Empty"); }

    const char* CardName = DORCard_GetName(CardId);
    if(CardName == nullptr) { CardName = "Unknown"; }
    return QStringLiteral("%1 - %2").arg(CardId, 3, 10, QChar('0')).arg(CardName);
}
