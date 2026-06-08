#include "dor_info_tab_model.h"

#include <limits>

namespace
{
QString BytesToHex(const uint8_t* Bytes, size_t ByteCount)
{
    QString Result;
    for(size_t ByteIndex = 0; ByteIndex < ByteCount; ++ByteIndex)
    {
        if(ByteIndex > 0) { Result.append(QLatin1Char(' ')); }
        Result.append(QStringLiteral("%1").arg(Bytes[ByteIndex], 2, 16, QChar('0')).toUpper());
    }
    return Result;
}

QString HexValue(qulonglong Value, int Width = 0)
{
    QString Hex = QString::number(Value, 16).toUpper();
    if(Width > 0) { Hex = Hex.rightJustified(Width, QChar('0')); }
    return QStringLiteral("0x%1").arg(Hex);
}

QString ByteCountValue(uint8_t Value)
{
    return QStringLiteral("%1 (%2)").arg(Value).arg(HexValue(Value, 2));
}

template<typename UInt>
bool ParseUnsignedValue(const QVariant& Value, UInt* pOutValue)
{
    if(pOutValue == nullptr) { return false; }

    constexpr qulonglong MaxValue = std::numeric_limits<UInt>::max();

    if(Value.canConvert<qulonglong>())
    {
        bool bOk = false;
        const qulonglong NumericValue = Value.toULongLong(&bOk);
        if(bOk && NumericValue <= MaxValue)
        {
            *pOutValue = static_cast<UInt>(NumericValue);
            return true;
        }
    }

    QString Text = Value.toString().trimmed();
    if(Text.isEmpty()) { return false; }

    const int ParenIndex = Text.indexOf(QLatin1Char('('));
    if(ParenIndex >= 0)
    {
        Text = Text.left(ParenIndex).trimmed();
        if(Text.isEmpty()) { return false; }
    }

    const QStringList ByteTokens = Text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if(ByteTokens.size() == static_cast<int>(sizeof(UInt)))
    {
        qulonglong NumericValue = 0;
        for(int Index = 0; Index < ByteTokens.size(); ++Index)
        {
            QString ByteText = ByteTokens[Index];
            if(ByteText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
            {
                ByteText = ByteText.mid(2);
            }

            bool bByteOk = false;
            const uint ByteValue = ByteText.toUInt(&bByteOk, 16);
            if(!bByteOk || ByteValue > 0xFFu) { return false; }
            NumericValue |= static_cast<qulonglong>(ByteValue) << (static_cast<uint>(Index) * 8u);
        }

        *pOutValue = static_cast<UInt>(NumericValue);
        return true;
    }

    int Base = 10;
    if(Text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
    {
        Text = Text.mid(2);
        Base = 16;
    }

    bool bOk = false;
    qulonglong NumericValue = Text.toULongLong(&bOk, Base);
    if(!bOk && Base == 10)
    {
        NumericValue = Text.toULongLong(&bOk, 16);
    }
    if(!bOk || NumericValue > MaxValue) { return false; }

    *pOutValue = static_cast<UInt>(NumericValue);
    return true;
}
}

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

void DORInfoTabModel::SetPath(const QString& InPath)
{
    if(Path == InPath) { return; }

    Path = InPath;

    const QModelIndex OpenedPathIndex = IndexForField(OpenedPathField, ValueColumn);
    emit dataChanged(OpenedPathIndex, OpenedPathIndex, { Qt::DisplayRole });
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
        if(row != 0) { return QModelIndex(); }

        switch(FieldId)
        {
            case PlayerNameField:
                return createIndex(row, column, MakeDetailNodeId(RawPlayerNameBytesDetail));

            case RecentCardsField:
                return createIndex(row, column, MakeDetailNodeId(RawRecentCardsBytesDetail));

            case MapLocationStateField:
                return createIndex(row, column, MakeDetailNodeId(RawMapLocationStateBytesDetail));

            default:
                return QModelIndex();
        }
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
            switch(static_cast<Detail>(NodeValue(child)))
            {
                case RawPlayerNameBytesDetail:
                    return IndexForField(PlayerNameField, 0);

                case RawRecentCardsBytesDetail:
                    return IndexForField(RecentCardsField, 0);

                case RawMapLocationStateBytesDetail:
                    return IndexForField(MapLocationStateField, 0);

                default:
                    return QModelIndex();
            }

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

    if(NodeType(parent) == FieldNode)
    {
        switch(static_cast<Field>(NodeValue(parent)))
        {
            case PlayerNameField:
            case RecentCardsField:
            case MapLocationStateField:
                return 1;

            default:
                return 0;
        }
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

        case ProgressGroup:
            return QStringLiteral("Career Progression");

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
        { ProfileTokenField, IntegrityGroup, "Profile Token" },

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

        { RecentCardsField, ProgressGroup, "Recent Cards" },
        { ProfileProgressStateField, ProgressGroup, "Profile State Bytes" },
        { FooterProgressStateField, ProgressGroup, "Footer State Bytes" },
        { MapLocationStateField, ProgressGroup, "Map Location State" },
        { CampaignProgressStateField, ProgressGroup, "Campaign State Bytes (Provisional)" },
        { PotentialCampaignSideFlagField, ProgressGroup, "Potential Campaign Side Flag" },
        { PotentialProfileLossCountField, ProgressGroup, "Potential Profile Loss Count" },
        { PotentialFooterLossCountField, ProgressGroup, "Potential Footer Loss Count" },
        { PotentialProfileDuelCountField, ProgressGroup, "Potential Profile Duel Count" },
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
            return Save == nullptr ? QStringLiteral("No save loaded") : QVariant();

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

        case ProfileTokenField:
            return ProfileToken();

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

        case RecentCardsField:
            return RecentCards();

        case ProfileProgressStateField:
            return ProgressProfileStateBytes();

        case FooterProgressStateField:
            return ProgressFooterStateBytes();

        case MapLocationStateField:
            return MapLocationState();

        case CampaignProgressStateField:
            return ProgressCampaignStateBytes();

        case PotentialCampaignSideFlagField:
            return PotentialCampaignSideFlag();

        case PotentialProfileLossCountField:
            return PotentialProfileLossCount();

        case PotentialFooterLossCountField:
            return PotentialFooterLossCount();

        case PotentialProfileDuelCountField:
            return PotentialProfileDuelCount();

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

        case RawRecentCardsBytesDetail:
            if(Column == NameColumn) { return Role == Qt::DisplayRole ? QStringLiteral("Raw Bytes") : QVariant(); }
            if(Column == ValueColumn) { return RawRecentCardsBytes(); }
            return QVariant();

        case RawMapLocationStateBytesDetail:
            if(Column == NameColumn) { return Role == Qt::DisplayRole ? QStringLiteral("Raw Bytes") : QVariant(); }
            if(Column == ValueColumn) { return RawMapLocationStateBytes(); }
            return QVariant();

        default:
            return QVariant();
    }
}

bool DORInfoTabModel::SetFieldData(Field FieldId, const QVariant& Value, int Role)
{
    if(Role != Qt::EditRole || Save == nullptr) { return false; }

    if(FieldId == PotentialCampaignSideFlagField ||
       FieldId == PotentialProfileLossCountField ||
       FieldId == PotentialProfileDuelCountField)
    {
        uint8_t NewValue = 0;
        if(!ParseUnsignedValue(Value, &NewValue)) { return false; }

        DORStatus Status = DORStatusInvalidArgument;
        switch(FieldId)
        {
            case PotentialCampaignSideFlagField:
                Status = DORSave_SetPotentialCampaignSideFlag(Save, NewValue);
                break;

            case PotentialProfileLossCountField:
                Status = DORSave_SetPotentialProfileLossCount(Save, NewValue);
                break;

            case PotentialProfileDuelCountField:
                Status = DORSave_SetPotentialProfileDuelCount(Save, NewValue);
                break;

            default:
                break;
        }
        if(Status != DORStatusOk) { return false; }

        const QModelIndex EditedIndex = IndexForField(FieldId, ValueColumn);
        const QModelIndex CampaignProgressStateIndex = IndexForField(CampaignProgressStateField, ValueColumn);
        const QModelIndex ChecksumIndex = IndexForField(ChecksumField, ValueColumn);
        emit dataChanged(EditedIndex, EditedIndex, { Qt::DisplayRole, Qt::EditRole });
        emit dataChanged(CampaignProgressStateIndex, CampaignProgressStateIndex, { Qt::DisplayRole });
        emit dataChanged(ChecksumIndex, ChecksumIndex, { Qt::DisplayRole });

        if(FieldId == PotentialCampaignSideFlagField)
        {
            const QModelIndex ProfileProgressStateIndex = IndexForField(ProfileProgressStateField, ValueColumn);
            emit dataChanged(ProfileProgressStateIndex, ProfileProgressStateIndex, { Qt::DisplayRole });
        }
        if(FieldId == PotentialProfileDuelCountField)
        {
            const QModelIndex FooterProgressStateIndex = IndexForField(FooterProgressStateField, ValueColumn);
            emit dataChanged(FooterProgressStateIndex, FooterProgressStateIndex, { Qt::DisplayRole });
        }
        return true;
    }

    if(FieldId == MapLocationStateField)
    {
        uint16_t NewValue = 0;
        if(!ParseUnsignedValue(Value, &NewValue)) { return false; }
        if(DORSave_SetMapLocationState(Save, NewValue) != DORStatusOk) { return false; }

        const QModelIndex MapLocationStateIndex = IndexForField(MapLocationStateField, ValueColumn);
        const QModelIndex RawMapLocationStateBytesIndex = index(0, ValueColumn, IndexForField(MapLocationStateField, 0));
        const QModelIndex FooterProgressStateIndex = IndexForField(FooterProgressStateField, ValueColumn);
        const QModelIndex ChecksumIndex = IndexForField(ChecksumField, ValueColumn);
        emit dataChanged(MapLocationStateIndex, MapLocationStateIndex, { Qt::DisplayRole, Qt::EditRole });
        emit dataChanged(RawMapLocationStateBytesIndex, RawMapLocationStateBytesIndex, { Qt::DisplayRole });
        emit dataChanged(FooterProgressStateIndex, FooterProgressStateIndex, { Qt::DisplayRole });
        emit dataChanged(ChecksumIndex, ChecksumIndex, { Qt::DisplayRole });
        return true;
    }

    if(FieldId != PlayerNameField) { return false; }

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
    if(Column == ValueColumn && Save != nullptr &&
       (FieldId == PlayerNameField ||
        FieldId == MapLocationStateField ||
        FieldId == PotentialCampaignSideFlagField ||
        FieldId == PotentialProfileLossCountField ||
        FieldId == PotentialProfileDuelCountField))
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

    return BytesToHex(Bytes, ByteCount);
}

QString DORInfoTabModel::Checksum() const
{
    if(Save == nullptr) { return QString(); }

    return HexValue(DORSave_GetChecksum(Save), 4);
}

QString DORInfoTabModel::ProfileToken() const
{
    if(Save == nullptr) { return QString(); }

    const uint8_t* Bytes = nullptr;
    size_t ByteCount = 0;
    if(DORSave_GetProfileTokenBytes(Save, &Bytes, &ByteCount) != DORStatusOk) { return QString(); }

    return BytesToHex(Bytes, ByteCount);
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

    return HexValue(Entry.PayloadOffset);
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

QString DORInfoTabModel::RecentCards() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    const uint16_t* CardIds = nullptr;
    size_t CardCount = 0;
    if(DORProgressInfo_GetRecentCards(&Info, &CardIds, &CardCount) != DORStatusOk) { return QString(); }

    QStringList Cards;
    for(size_t Index = 0; Index < CardCount; ++Index)
    {
        const uint16_t CardId = CardIds[Index];
        if(CardId == DOREmptyCardId) { continue; }

        const char* Name = DORCard_GetName(CardId);
        Cards.append(QStringLiteral("%1 - %2")
            .arg(CardId, 3, 10, QChar('0'))
            .arg(Name == nullptr ? "Unknown" : Name));
    }

    return Cards.isEmpty() ? QStringLiteral("None") : Cards.join(QStringLiteral(", "));
}

QString DORInfoTabModel::RawRecentCardsBytes() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    const uint16_t* CardIds = nullptr;
    size_t CardCount = 0;
    if(DORProgressInfo_GetRecentCards(&Info, &CardIds, &CardCount) != DORStatusOk) { return QString(); }

    uint8_t Bytes[DORProgressRecentCardCount * 2u] = {};
    for(size_t Index = 0; Index < CardCount; ++Index)
    {
        const uint16_t CardId = CardIds[Index];
        Bytes[Index * 2u] = static_cast<uint8_t>(CardId & 0xFFu);
        Bytes[Index * 2u + 1u] = static_cast<uint8_t>((CardId >> 8u) & 0xFFu);
    }

    return BytesToHex(Bytes, sizeof(Bytes));
}

QString DORInfoTabModel::ProgressProfileStateBytes() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    return BytesToHex(Info.ProfileStateBytes, DORProgressProfileStateByteCount);
}

QString DORInfoTabModel::ProgressFooterStateBytes() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    return BytesToHex(Info.FooterStateBytes, DORProgressFooterStateByteCount);
}

QString DORInfoTabModel::MapLocationState() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    const uint16_t Value = DORProgressInfo_GetMapLocationState(&Info);
    return HexValue(Value, 4);
}

QString DORInfoTabModel::ProgressCampaignStateBytes() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    const uint8_t* Bytes = nullptr;
    size_t ByteCount = 0;
    if(DORProgressInfo_GetCampaignStateBytes(&Info, &Bytes, &ByteCount) != DORStatusOk) { return QString(); }

    return BytesToHex(Bytes, ByteCount);
}

QString DORInfoTabModel::PotentialCampaignSideFlag() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    return ByteCountValue(DORProgressInfo_GetPotentialCampaignSideFlag(&Info));
}

QString DORInfoTabModel::PotentialProfileLossCount() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    return ByteCountValue(DORProgressInfo_GetPotentialProfileLossCount(&Info));
}

QString DORInfoTabModel::PotentialFooterLossCount() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    return ByteCountValue(DORProgressInfo_GetPotentialFooterLossCount(&Info));
}

QString DORInfoTabModel::PotentialProfileDuelCount() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    return ByteCountValue(DORProgressInfo_GetPotentialProfileDuelCount(&Info));
}

QString DORInfoTabModel::RawMapLocationStateBytes() const
{
    DORProgressInfo Info = {};
    if(!GetProgressInfo(&Info)) { return QString(); }

    return BytesToHex(Info.FooterStateBytes, 2u);
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

bool DORInfoTabModel::GetProgressInfo(DORProgressInfo* pOutInfo) const
{
    if(Save == nullptr || pOutInfo == nullptr) { return false; }

    return DORSave_GetProgressInfo(Save, pOutInfo) == DORStatusOk;
}

bool DORInfoTabModel::GetGameDataEntry(PSUEntryInfo* pOutEntry) const
{
    if(Archive == nullptr || pOutEntry == nullptr) { return false; }

    return PSUArchive_FindFile(Archive, DORSaveFileNameNTSC, pOutEntry) == PSUStatusOk;
}
