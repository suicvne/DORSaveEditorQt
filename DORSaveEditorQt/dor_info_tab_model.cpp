#include "dor_info_tab_model.h"

DORInfoTabModel::DORInfoTabModel(QObject *parent)
    : QAbstractItemModel{parent}
{}

void DORInfoTabModel::SetContext(DORSave* InSave, const PSUArchive* InArchive, const QString& InPath)
{
    beginResetModel();
    Save = InSave;
    Archive = InArchive;
    Path = InPath;
    endResetModel();
}

DORSave* DORInfoTabModel::GetSave() const
{
    return Save;
}

const PSUArchive* DORInfoTabModel::GetArchive() const
{
    return Archive;
}

QString DORInfoTabModel::GetPath() const
{
    return Path;
}

QModelIndex DORInfoTabModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) { return QModelIndex(); }

    if(!parent.isValid())
    {
        if(row < 0 || row >= GroupCount) { return QModelIndex(); }
        return createIndex(row, column, MakeGroupNodeId(static_cast<Group>(row)));
    }

    if(NodeType(parent) == FieldNode)
    {
        const Field FieldId = static_cast<Field>(NodeValue(parent));
        if(FieldId != PlayerNameField || row != 0) { return QModelIndex(); }
        return createIndex(row, column, MakeDetailNodeId(RawPlayerNameBytesDetail));
    }

    if(NodeType(parent) != GroupNode) { return QModelIndex(); }

    const Group GroupId = static_cast<Group>(NodeValue(parent));
    const FieldDefinition* Definition = FieldForRow(GroupId, row);
    if(Definition == nullptr) { return QModelIndex(); }

    return createIndex(row, column, MakeFieldNodeId(Definition->Id));
}

QModelIndex DORInfoTabModel::parent(const QModelIndex& child) const
{
    if(!child.isValid()) { return QModelIndex(); }

    switch(NodeType(child))
    {
        case GroupNode:
            return QModelIndex();

        case FieldNode:
        {
            const FieldDefinition* Definition = FindField(static_cast<Field>(NodeValue(child)));
            if(Definition == nullptr) { return QModelIndex(); }
            return createIndex(static_cast<int>(Definition->ParentGroup), 0, MakeGroupNodeId(Definition->ParentGroup));
        }

        case DetailNode:
            if(static_cast<Detail>(NodeValue(child)) == RawPlayerNameBytesDetail)
            {
                return IndexForField(PlayerNameField, 0);
            }
            return QModelIndex();

        default:
            return QModelIndex();
    }
}

int DORInfoTabModel::rowCount(const QModelIndex& parent) const
{
    if(parent.column() > 0) { return 0; }

    if(!parent.isValid()) { return GroupCount; }

    if(NodeType(parent) == GroupNode)
    {
        return FieldCountForGroup(static_cast<Group>(NodeValue(parent)));
    }

    if(NodeType(parent) == FieldNode && static_cast<Field>(NodeValue(parent)) == PlayerNameField)
    {
        return 1;
    }

    return 0;
}

int DORInfoTabModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant DORInfoTabModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) { return QVariant(); }
    if(role != Qt::DisplayRole && role != Qt::EditRole) { return QVariant(); }

    switch(NodeType(index))
    {
        case GroupNode:
            return role == Qt::DisplayRole ? GroupData(static_cast<Group>(NodeValue(index)), index.column()) : QVariant();

        case FieldNode:
            return FieldData(static_cast<Field>(NodeValue(index)), index.column(), role);

        case DetailNode:
            return DetailData(static_cast<Detail>(NodeValue(index)), index.column(), role);

        default:
            return QVariant();
    }
}

bool DORInfoTabModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid() || NodeType(index) != FieldNode || index.column() != ValueColumn) { return false; }

    return SetFieldData(static_cast<Field>(NodeValue(index)), value, role);
}

Qt::ItemFlags DORInfoTabModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) { return Qt::NoItemFlags; }

    Qt::ItemFlags Result = QAbstractItemModel::flags(index);
    if(NodeType(index) == FieldNode)
    {
        Result |= FieldFlags(static_cast<Field>(NodeValue(index)), index.column());
    }
    return Result;
}

QVariant DORInfoTabModel::headerData(int section, Qt::Orientation orientation, int role) const
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

quintptr DORInfoTabModel::MakeGroupNodeId(Group GroupId)
{
    return (static_cast<quintptr>(GroupId) << 2) | static_cast<quintptr>(GroupNode);
}

quintptr DORInfoTabModel::MakeFieldNodeId(Field FieldId)
{
    return (static_cast<quintptr>(FieldId) << 2) | static_cast<quintptr>(FieldNode);
}

quintptr DORInfoTabModel::MakeDetailNodeId(Detail DetailId)
{
    return (static_cast<quintptr>(DetailId) << 2) | static_cast<quintptr>(DetailNode);
}

DORInfoTabModel::ModelNodeType DORInfoTabModel::NodeType(const QModelIndex& Index)
{
    return Index.isValid() ? static_cast<ModelNodeType>(Index.internalId() & 0x3u) : InvalidNode;
}

uint DORInfoTabModel::NodeValue(const QModelIndex& Index)
{
    return static_cast<uint>(Index.internalId() >> 2);
}

QString DORInfoTabModel::GroupName(Group GroupId)
{
    switch(GroupId)
    {
        case IdentityGroup:
            return QStringLiteral("Identity");

        case IntegrityGroup:
            return QStringLiteral("Integrity");

        case FileGroup:
            return QStringLiteral("File");

        case CollectionGroup:
            return QStringLiteral("Collection Summary");

        case DeckGroup:
            return QStringLiteral("Deck Summary");

        default:
            return QStringLiteral("Unknown");
    }
}

const DORInfoTabModel::FieldDefinition* DORInfoTabModel::FieldDefinitions(size_t* pOutCount)
{
    static const FieldDefinition Definitions[] = {
        { PlayerNameField, IdentityGroup, "Player Name" },
        { MaxNameLengthField, IdentityGroup, "Max Name Length" },
        { SaveEntryNameField, IdentityGroup, "Save Entry Name" },

        { ChecksumField, IntegrityGroup, "Checksum" },

        { OpenedPathField, FileGroup, "Opened Path" },
        { ContainerTypeField, FileGroup, "Container Type" },
        { GameDataSizeField, FileGroup, "Game Data Size" },
        { PsuEntryCountField, FileGroup, "PSU Entry Count" },
        { GameDataEntryOffsetField, FileGroup, "Game Data Entry Offset" },
        { GameDataEntrySizeField, FileGroup, "Game Data Entry Size" },

        { UniqueOwnedCardsField, CollectionGroup, "Unique Owned Cards" },
        { TotalOwnedCopiesField, CollectionGroup, "Total Owned Copies" },
        { ChestCopiesField, CollectionGroup, "Chest Copies" },
        { DeckCopiesField, CollectionGroup, "Deck Copies" },
        { LeaderMarkersField, CollectionGroup, "Leader Markers" },

        { DeckAField, DeckGroup, "Deck A" },
        { DeckBField, DeckGroup, "Deck B" },
        { DeckCField, DeckGroup, "Deck C" },
    };

    if(pOutCount != nullptr) { *pOutCount = sizeof(Definitions) / sizeof(Definitions[0]); }
    return Definitions;
}

const DORInfoTabModel::FieldDefinition* DORInfoTabModel::FieldForRow(Group GroupId, int Row)
{
    if(Row < 0) { return nullptr; }

    size_t DefinitionCount = 0;
    const FieldDefinition* Definitions = FieldDefinitions(&DefinitionCount);
    int CurrentRow = 0;
    for(size_t Index = 0; Index < DefinitionCount; ++Index)
    {
        const FieldDefinition& Definition = Definitions[Index];
        if(Definition.ParentGroup != GroupId) { continue; }
        if(CurrentRow == Row) { return &Definition; }
        ++CurrentRow;
    }

    return nullptr;
}

const DORInfoTabModel::FieldDefinition* DORInfoTabModel::FindField(Field FieldId)
{
    size_t DefinitionCount = 0;
    const FieldDefinition* Definitions = FieldDefinitions(&DefinitionCount);
    for(size_t Index = 0; Index < DefinitionCount; ++Index)
    {
        const FieldDefinition& Definition = Definitions[Index];
        if(Definition.Id == FieldId) { return &Definition; }
    }

    return nullptr;
}

int DORInfoTabModel::FieldRow(Field FieldId)
{
    const FieldDefinition* Target = FindField(FieldId);
    if(Target == nullptr) { return -1; }

    size_t DefinitionCount = 0;
    const FieldDefinition* Definitions = FieldDefinitions(&DefinitionCount);
    int Row = 0;
    for(size_t Index = 0; Index < DefinitionCount; ++Index)
    {
        const FieldDefinition& Definition = Definitions[Index];
        if(Definition.ParentGroup != Target->ParentGroup) { continue; }
        if(Definition.Id == FieldId) { return Row; }
        ++Row;
    }

    return -1;
}

int DORInfoTabModel::FieldCountForGroup(Group GroupId)
{
    size_t DefinitionCount = 0;
    const FieldDefinition* Definitions = FieldDefinitions(&DefinitionCount);
    int Count = 0;
    for(size_t Index = 0; Index < DefinitionCount; ++Index)
    {
        const FieldDefinition& Definition = Definitions[Index];
        if(Definition.ParentGroup == GroupId) { ++Count; }
    }

    return Count;
}

QModelIndex DORInfoTabModel::IndexForField(Field FieldId, int Column) const
{
    const FieldDefinition* Definition = FindField(FieldId);
    const int Row = FieldRow(FieldId);
    if(Definition == nullptr || Row < 0) { return QModelIndex(); }

    return createIndex(Row, Column, MakeFieldNodeId(FieldId));
}

QVariant DORInfoTabModel::GroupData(Group GroupId, int Column) const
{
    switch(Column)
    {
        case NameColumn:
            return GroupName(GroupId);

        case ValueColumn:
            return Save == nullptr ? QStringLiteral("No save loaded") : QStringLiteral("%1 fields").arg(FieldCountForGroup(GroupId));

        default:
            return QVariant();
    }
}

QVariant DORInfoTabModel::FieldData(Field FieldId, int Column, int Role) const
{
    const FieldDefinition* Definition = FindField(FieldId);
    if(Definition == nullptr) { return QVariant(); }

    if(Column == NameColumn)
    {
        return Role == Qt::DisplayRole ? QString::fromLatin1(Definition->Name) : QVariant();
    }
    if(Column != ValueColumn) { return QVariant(); }

    switch(FieldId)
    {
        case PlayerNameField:
            return PlayerName();

        case MaxNameLengthField:
            return DORNameCharacterCount;

        case SaveEntryNameField:
            return SaveEntryName();

        case ChecksumField:
            return Checksum();

        case OpenedPathField:
            return Path.isEmpty() ? QVariant() : Path;

        case ContainerTypeField:
            return Archive == nullptr ? QVariant() : QStringLiteral("PSU");

        case GameDataSizeField:
            return Save == nullptr ? QVariant() : QStringLiteral("%1 bytes").arg(DORSave_GetSize(Save));

        case PsuEntryCountField:
            return Archive == nullptr ? QVariant() : QVariant::fromValue(static_cast<qulonglong>(PSUArchive_GetEntryCount(Archive)));

        case GameDataEntryOffsetField:
            return GameDataEntryOffset();

        case GameDataEntrySizeField:
            return GameDataEntrySize();

        case UniqueOwnedCardsField:
        case TotalOwnedCopiesField:
        case ChestCopiesField:
        case DeckCopiesField:
        case LeaderMarkersField:
            return CollectionField(FieldId);

        case DeckAField:
            return DeckSummary(0);

        case DeckBField:
            return DeckSummary(1);

        case DeckCField:
            return DeckSummary(2);

        default:
            return QVariant();
    }
}

QVariant DORInfoTabModel::DetailData(Detail DetailId, int Column, int Role) const
{
    if(Role != Qt::DisplayRole && Role != Qt::EditRole) { return QVariant(); }

    switch(DetailId)
    {
        case RawPlayerNameBytesDetail:
            if(Column == NameColumn) { return Role == Qt::DisplayRole ? QStringLiteral("Raw Bytes") : QVariant(); }
            if(Column == ValueColumn) { return RawPlayerNameBytes(); }
            return QVariant();

        default:
            return QVariant();
    }
}

bool DORInfoTabModel::SetFieldData(Field FieldId, const QVariant& Value, int Role)
{
    if(Role != Qt::EditRole || FieldId != PlayerNameField || Save == nullptr) { return false; }

    const QString NewName = Value.toString();
    // if(NewName == PlayerName()) { return true; }

    const QByteArray EncodedName = NewName.toUtf8();
    if(DORSave_SetPlayerName(Save, EncodedName.constData()) != DORStatusOk) { return false; }

    const QModelIndex PlayerNameIndex = IndexForField(PlayerNameField, ValueColumn);
    const QModelIndex RawNameBytesIndex = index(0, ValueColumn, IndexForField(PlayerNameField, 0));
    const QModelIndex ChecksumIndex = IndexForField(ChecksumField, ValueColumn);
    emit dataChanged(PlayerNameIndex, PlayerNameIndex, { Qt::DisplayRole, Qt::EditRole });
    emit dataChanged(RawNameBytesIndex, RawNameBytesIndex, { Qt::DisplayRole });
    emit dataChanged(ChecksumIndex, ChecksumIndex, { Qt::DisplayRole });
    return true;
}

Qt::ItemFlags DORInfoTabModel::FieldFlags(Field FieldId, int Column) const
{
    if(FieldId == PlayerNameField && Column == ValueColumn && Save != nullptr)
    {
        return Qt::ItemIsEditable;
    }

    return Qt::NoItemFlags;
}

QString DORInfoTabModel::PlayerName() const
{
    if(Save == nullptr) { return QString(); }

    char Name[DORNameCharacterCount + 1u] = {};
    if(DORSave_GetPlayerName(Save, Name, sizeof(Name)) != DORStatusOk) { return QString(); }

    return QString::fromLatin1(Name);
}

QString DORInfoTabModel::RawPlayerNameBytes() const
{
    if(Save == nullptr) { return QString(); }

    const uint8_t* Bytes = nullptr;
    size_t ByteCount = 0;
    if(DORSave_GetRawPlayerNameBytes(Save, &Bytes, &ByteCount) != DORStatusOk) { return QString(); }

    QString Result;
    for(size_t ByteIndex = 0; ByteIndex < ByteCount; ++ByteIndex)
    {
        if(ByteIndex > 0) { Result.append(QLatin1Char(' ')); }
        Result.append(QStringLiteral("%1").arg(Bytes[ByteIndex], 2, 16, QChar('0')).toUpper());
    }

    return Result;
}

QString DORInfoTabModel::Checksum() const
{
    if(Save == nullptr) { return QString(); }

    const QString Value = QString::number(DORSave_GetChecksum(Save), 16).toUpper().rightJustified(4, QChar('0'));
    return QStringLiteral("0x%1").arg(Value);
}

QString DORInfoTabModel::SaveEntryName() const
{
    PSUEntryInfo Entry = {};
    if(!GetGameDataEntry(&Entry)) { return QString(); }

    return QString::fromLatin1(Entry.Name);
}

QString DORInfoTabModel::GameDataEntryOffset() const
{
    PSUEntryInfo Entry = {};
    if(!GetGameDataEntry(&Entry)) { return QString(); }

    const QString Value = QString::number(Entry.PayloadOffset, 16).toUpper();
    return QStringLiteral("0x%1").arg(Value);
}

QString DORInfoTabModel::GameDataEntrySize() const
{
    PSUEntryInfo Entry = {};
    if(!GetGameDataEntry(&Entry)) { return QString(); }

    return QStringLiteral("%1 bytes").arg(Entry.Size);
}

QVariant DORInfoTabModel::CollectionField(Field FieldId) const
{
    if(Save == nullptr) { return QVariant(); }

    const CollectionSummary Summary = BuildCollectionSummary();
    switch(FieldId)
    {
        case UniqueOwnedCardsField:
            return Summary.UniqueOwnedCards;

        case TotalOwnedCopiesField:
            return Summary.TotalOwnedCopies;

        case ChestCopiesField:
            return Summary.ChestCopies;

        case DeckCopiesField:
            return Summary.DeckCopies;

        case LeaderMarkersField:
            return Summary.LeaderMarkers;

        default:
            return QVariant();
    }
}

QString DORInfoTabModel::DeckSummary(uint DeckIndex) const
{
    if(Save == nullptr || DeckIndex >= 3) { return QString(); }

    DORDeckInfo Info = {};
    if(DORSave_GetDeckInfo(Save, static_cast<DORDeckID>(DeckIndex), &Info) != DORStatusOk) { return QString(); }

    uint FilledSlots = 0;
    for(uint CardIndex = 0; CardIndex < DORDeckCardCount; ++CardIndex)
    {
        if(Info.Cards[CardIndex] != DOREmptyCardId) { ++FilledSlots; }
    }

    const char* LeaderName = DORCard_GetName(Info.LeaderCardId);
    if(Info.LeaderCardId == DOREmptyCardId)
    {
        LeaderName = "Empty";
    }
    else if(LeaderName == nullptr)
    {
        LeaderName = "Unknown";
    }

    return QStringLiteral("leader %1 - %2, cards %3/%4, stored cost %5")
        .arg(Info.LeaderCardId, 3, 10, QChar('0'))
        .arg(LeaderName)
        .arg(FilledSlots)
        .arg(DORDeckCardCount)
        .arg(Info.StoredDeckCost);
}

DORInfoTabModel::CollectionSummary DORInfoTabModel::BuildCollectionSummary() const
{
    CollectionSummary Summary;
    if(Save == nullptr) { return Summary; }

    for(uint CardId = 0; CardId < DORCardCount; ++CardId)
    {
        DORCardInfo Info = {};
        if(DORSave_GetCardInfo(Save, static_cast<uint16_t>(CardId), &Info) != DORStatusOk) { continue; }

        const uint TotalCopies = DORCardInfo_GetTotalCopyCount(&Info);
        if(TotalCopies > 0) { ++Summary.UniqueOwnedCards; }

        Summary.TotalOwnedCopies += TotalCopies;
        Summary.ChestCopies += DORCardInfo_GetChestCopyCount(&Info);
        Summary.DeckCopies += DORCardInfo_GetDeckCopyCount(&Info);
        Summary.LeaderMarkers += DORCardInfo_GetLeaderMarkerCount(&Info);
    }

    return Summary;
}

bool DORInfoTabModel::GetGameDataEntry(PSUEntryInfo* pOutEntry) const
{
    if(Archive == nullptr || pOutEntry == nullptr) { return false; }

    return PSUArchive_FindFile(Archive, DORSaveFileNameNTSC, pOutEntry) == PSUStatusOk;
}
