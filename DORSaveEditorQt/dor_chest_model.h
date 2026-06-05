#ifndef DOR_CHEST_MODEL_H
#define DOR_CHEST_MODEL_H

#include <DOR.h>

#include <QAbstractItemModel>

class DORChestModel : public QAbstractItemModel
{
public:
    explicit DORChestModel(QObject *parent = nullptr);

    enum Column {
        NameColumn,
        ValueColumn,
        ColumnCount
    };

    void SetSave(const DORSave* InSave);
    const DORSave* GetSave() const;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    enum ModelNodeType : quintptr {
        InvalidNode = 0,
        ChestNode = 1,
        CardNode = 2,
        CopySlotNode = 3
    };

    static quintptr MakeNodeId(ModelNodeType Type, uint Value);
    static ModelNodeType NodeType(const QModelIndex& Index);
    static uint NodeValue(const QModelIndex& Index);

    QVariant CardData(uint CardId, int Column) const;
    QVariant CopySlotData(uint SlotValue, int Column) const;
    static QString FormatCopySlot(const DORCopySlot& Slot);

    const DORSave* Save = nullptr;
};

#endif // DOR_CHEST_MODEL_H
