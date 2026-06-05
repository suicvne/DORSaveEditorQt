#ifndef DOR_INFO_TAB_MODEL_H
#define DOR_INFO_TAB_MODEL_H

#include <DOR.h>
#include <PSU.h>

#include <QAbstractItemModel>
#include <QString>

class DORInfoTabModel : public QAbstractItemModel
{
public:
    explicit DORInfoTabModel(QObject *parent = nullptr);

    enum Column {
        NameColumn,
        ValueColumn,
        ColumnCount
    };

    void SetContext(DORSave* InSave, const PSUArchive* InArchive, const QString& InPath);
    DORSave* GetSave() const;
    const PSUArchive* GetArchive() const;
    QString GetPath() const;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    enum ModelNodeType : quintptr {
        InvalidNode = 0,
        GroupNode = 1,
        FieldNode = 2,
        DetailNode = 3
    };

    enum Group {
        IdentityGroup,
        IntegrityGroup,
        FileGroup,
        CollectionGroup,
        DeckGroup,
        GroupCount
    };

    enum Field {
        PlayerNameField,
        MaxNameLengthField,
        SaveEntryNameField,

        ChecksumField,

        OpenedPathField,
        ContainerTypeField,
        GameDataSizeField,
        PsuEntryCountField,
        GameDataEntryOffsetField,
        GameDataEntrySizeField,

        UniqueOwnedCardsField,
        TotalOwnedCopiesField,
        ChestCopiesField,
        DeckCopiesField,
        LeaderMarkersField,

        DeckAField,
        DeckBField,
        DeckCField,

        FieldCount
    };

    enum Detail {
        RawPlayerNameBytesDetail,
        DetailCount
    };

    struct FieldDefinition {
        Field Id;
        Group ParentGroup;
        const char* Name;
    };

    struct CollectionSummary {
        uint UniqueOwnedCards = 0;
        uint TotalOwnedCopies = 0;
        uint ChestCopies = 0;
        uint DeckCopies = 0;
        uint LeaderMarkers = 0;
    };

    static quintptr MakeGroupNodeId(Group GroupId);
    static quintptr MakeFieldNodeId(Field FieldId);
    static quintptr MakeDetailNodeId(Detail DetailId);
    static ModelNodeType NodeType(const QModelIndex& Index);
    static uint NodeValue(const QModelIndex& Index);
    static QString GroupName(Group GroupId);
    static const FieldDefinition* FieldDefinitions(size_t* pOutCount);
    static const FieldDefinition* FieldForRow(Group GroupId, int Row);
    static const FieldDefinition* FindField(Field FieldId);
    static int FieldRow(Field FieldId);
    static int FieldCountForGroup(Group GroupId);

    QModelIndex IndexForField(Field FieldId, int Column) const;
    QVariant GroupData(Group GroupId, int Column) const;
    QVariant FieldData(Field FieldId, int Column, int Role) const;
    QVariant DetailData(Detail DetailId, int Column, int Role) const;
    bool SetFieldData(Field FieldId, const QVariant& Value, int Role);
    Qt::ItemFlags FieldFlags(Field FieldId, int Column) const;

    QString PlayerName() const;
    QString RawPlayerNameBytes() const;
    QString Checksum() const;
    QString SaveEntryName() const;
    QString GameDataEntryOffset() const;
    QString GameDataEntrySize() const;
    QVariant CollectionField(Field FieldId) const;
    QString DeckSummary(uint DeckIndex) const;

    CollectionSummary BuildCollectionSummary() const;
    bool GetGameDataEntry(PSUEntryInfo* pOutEntry) const;

    DORSave* Save = nullptr;
    const PSUArchive* Archive = nullptr;
    QString Path;
};

#endif // DOR_INFO_TAB_MODEL_H
