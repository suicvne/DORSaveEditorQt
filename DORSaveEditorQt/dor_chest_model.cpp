#include "dor_chest_model.h"

#include <QString>

DORChestModel::DORChestModel(QObject *parent)
    : QAbstractItemModel{parent}
{

}

void DORChestModel::SetSave(const DORSave* InSave)
{
    beginResetModel();
    Save = InSave;
    endResetModel();
}

const DORSave* DORChestModel::GetSave() const
{
    return Save;
}

QModelIndex DORChestModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) { return QModelIndex(); }

    if(!parent.isValid())
    {
        if(row != 0) { return QModelIndex(); }
        return createIndex(row, column, MakeNodeId(ChestNode, 0));
    }

    switch(NodeType(parent))
    {
        case ChestNode:
            if(Save == nullptr || row < 0 || row >= static_cast<int>(DORCardCount)) { return QModelIndex(); }
            return createIndex(row, column, MakeNodeId(CardNode, static_cast<uint>(row)));

        case CardNode:
        {
            const uint CardId = NodeValue(parent);
            if(Save == nullptr || CardId >= DORCardCount || row < 0 || row >= static_cast<int>(DORCardCopySlotCount)) { return QModelIndex(); }
            return createIndex(row, column, MakeNodeId(CopySlotNode, CardId * DORCardCopySlotCount + static_cast<uint>(row)));
        }

        case CopySlotNode:
        default:
            return QModelIndex();
    }
}

QModelIndex DORChestModel::parent(const QModelIndex& child) const
{
    if(!child.isValid()) { return QModelIndex(); }

    switch(NodeType(child))
    {
        case ChestNode:
            return QModelIndex();

        case CardNode:
            return createIndex(0, 0, MakeNodeId(ChestNode, 0));

        case CopySlotNode:
        {
            const uint SlotValue = NodeValue(child);
            const uint CardId = SlotValue / DORCardCopySlotCount;
            return createIndex(static_cast<int>(CardId), 0, MakeNodeId(CardNode, CardId));
        }

        default:
            return QModelIndex();
    }
}

int DORChestModel::rowCount(const QModelIndex& parent) const
{
    if(parent.column() > 0) { return 0; }

    if(!parent.isValid()) { return 1; }

    switch(NodeType(parent))
    {
        case ChestNode:
            return Save == nullptr ? 0 : static_cast<int>(DORCardCount);

        case CardNode:
            return Save == nullptr ? 0 : static_cast<int>(DORCardCopySlotCount);

        case CopySlotNode:
        default:
            return 0;
    }
}

int DORChestModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant DORChestModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || role != Qt::DisplayRole) { return QVariant(); }

    switch(NodeType(index))
    {
        case ChestNode:
            if(index.column() == NameColumn) { return QStringLiteral("Chest"); }
            if(index.column() == ValueColumn) { return Save == nullptr ? QStringLiteral("No save loaded") : QStringLiteral("%1 cards").arg(DORCardCount); }
            return QVariant();

        case CardNode:
            return CardData(NodeValue(index), index.column());

        case CopySlotNode:
            return CopySlotData(NodeValue(index), index.column());

        default:
            return QVariant();
    }
}

QVariant DORChestModel::headerData(int section, Qt::Orientation orientation, int role) const
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

quintptr DORChestModel::MakeNodeId(ModelNodeType Type, uint Value)
{
    return (static_cast<quintptr>(Value) << 2) | static_cast<quintptr>(Type);
}

DORChestModel::ModelNodeType DORChestModel::NodeType(const QModelIndex& Index)
{
    return Index.isValid() ? static_cast<ModelNodeType>(Index.internalId() & 0x3u) : InvalidNode;
}

uint DORChestModel::NodeValue(const QModelIndex& Index)
{
    return static_cast<uint>(Index.internalId() >> 2);
}

QVariant DORChestModel::CardData(uint CardId, int Column) const
{
    if(Save == nullptr || CardId >= DORCardCount) { return QVariant(); }

    DORCardInfo Info = {};
    if(DORSave_GetCardInfo(Save, static_cast<uint16_t>(CardId), &Info) != DORStatusOk) { return QVariant(); }

    switch(Column)
    {
        case NameColumn:
        {
            const char* CardName = DORCard_GetName(static_cast<uint16_t>(CardId));
            if(CardName == nullptr) { CardName = "Unknown"; }
            return QStringLiteral("%1 - %2").arg(CardId, 3, 10, QChar('0')).arg(CardName);
        }

        case ValueColumn:
            return QStringLiteral("total %1, chest %2, deck %3, leader %4")
                .arg(DORCardInfo_GetTotalCopyCount(&Info))
                .arg(DORCardInfo_GetChestCopyCount(&Info))
                .arg(DORCardInfo_GetDeckCopyCount(&Info))
                .arg(DORCardInfo_GetLeaderMarkerCount(&Info));

        default:
            return QVariant();
    }
}

QVariant DORChestModel::CopySlotData(uint SlotValue, int Column) const
{
    if(Save == nullptr) { return QVariant(); }

    const uint CardId = SlotValue / DORCardCopySlotCount;
    const uint SlotIndex = SlotValue % DORCardCopySlotCount;
    if(CardId >= DORCardCount || SlotIndex >= DORCardCopySlotCount) { return QVariant(); }

    DORCardInfo Info = {};
    if(DORSave_GetCardInfo(Save, static_cast<uint16_t>(CardId), &Info) != DORStatusOk) { return QVariant(); }

    switch(Column)
    {
        case NameColumn:
            return QStringLiteral("Slot %1").arg(SlotIndex);

        case ValueColumn:
            return FormatCopySlot(Info.CopySlots[SlotIndex]);

        default:
            return QVariant();
    }
}

QString DORChestModel::FormatCopySlot(const DORCopySlot& Slot)
{
    QString Result;
    for(uint ByteIndex = 0; ByteIndex < DORCardCopySlotByteCount; ++ByteIndex)
    {
        if(ByteIndex > 0) { Result.append(QLatin1Char(' ')); }
        Result.append(QStringLiteral("%1").arg(Slot.Bytes[ByteIndex], 2, 16, QChar('0')).toUpper());
    }
    return Result;
}
