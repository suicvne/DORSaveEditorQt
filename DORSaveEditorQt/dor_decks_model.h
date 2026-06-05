#ifndef DOR_DECKS_MODEL_H
#define DOR_DECKS_MODEL_H

#include <DOR.h>

#include <QAbstractItemModel>

class DORDecksModel : public QAbstractItemModel
{
public:
    explicit DORDecksModel(QObject *parent = nullptr);

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
        DecksNode = 1,
        DeckNode = 2,
        LeaderNode = 3,
        CardNode = 4
    };

    static quintptr MakeNodeId(ModelNodeType Type, uint Value);
    static ModelNodeType NodeType(const QModelIndex& Index);
    static uint NodeValue(const QModelIndex& Index);

    QVariant DeckData(uint DeckIndex, int Column) const;
    QVariant LeaderData(uint DeckIndex, int Column) const;
    QVariant CardData(uint CardValue, int Column) const;
    static QString DeckName(uint DeckIndex);
    static QString FormatCard(uint16_t CardId);

    const DORSave* Save = nullptr;
};

#endif // DOR_DECKS_MODEL_H
