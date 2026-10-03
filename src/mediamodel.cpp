#include "mediamodel.h"
#include "mediamanager.h"

#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

namespace {
QString durationText(uint seconds)
{
    if (!seconds)
        return QStringLiteral("—");
    const uint minutes = seconds / 60;
    if (minutes < 60)
        return QStringLiteral("%1:%2").arg(minutes).arg(seconds % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2:%3")
        .arg(minutes / 60)
        .arg(minutes % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString availableText(const QString &text)
{
    return text.trimmed().isEmpty() ? QStringLiteral("—") : text;
}
}

MediaModel::MediaModel(MediaManager *mediaManager, CHANNEL_TYPE type, QObject *parent) :
    QAbstractTableModel(parent),
    mMediaManager_(mediaManager),
    mManagerType(type)
{
    if (mMediaManager_)
        mMediaManager_->setMediaModel(this);
    if (parent)
        connect(this, SIGNAL(dropFileList(QStringList)), parent, SLOT(copyFiles(QStringList)));
}

MediaModel::~MediaModel()
{
    if (mMediaManager_)
        mMediaManager_->setMediaModel(nullptr);
}

void MediaModel::setMediaManager(MediaManager *mediaManager)
{
    if (mMediaManager_ == mediaManager)
        return;
    beginCollect();
    if (mMediaManager_ && mMediaManager_ != mediaManager)
        mMediaManager_->setMediaModel(nullptr);
    mMediaManager_ = mediaManager;
    if (mMediaManager_)
        mMediaManager_->setMediaModel(this);
    endCollect();
}

QVariant MediaModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Vertical)
        return section >= 0 && section < rowCount() ? QVariant(section + 1) : QVariant();
    switch (section) {
    case FileNameColumn: return tr("Название");
    case TitleColumn: return tr("Название в тегах");
    case ArtistColumn: return tr("Исполнитель");
    case AlbumColumn: return tr("Альбом");
    case DurationColumn: return tr("Время");
    case FormatColumn: return tr("Формат");
    default: return {};
    }
}

QVariant MediaModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || !mMediaManager_
        || index.row() < 0 || index.row() >= rowCount()
        || index.column() < 0 || index.column() >= ColumnCount)
        return {};

    const MediaData &media = mMediaManager_->mediaData(index.row());
    switch (role) {
    case FileNameRole: return media.fileName();
    case TitleRole: return media.title();
    case ArtistRole: return media.artist();
    case AlbumRole: return media.album();
    case GenreRole: return media.genre();
    case YearRole: return media.year();
    case DurationSecondsRole: return media.length();
    case FormatRole: return QFileInfo(media.fileName()).suffix().toUpper();
    case FileSizeRole: return media.fileSize();
    case MediaTypeRole: return static_cast<int>(mManagerType);
    case Qt::TextAlignmentRole:
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::ToolTipRole:
    case Qt::AccessibleDescriptionRole:
        return tr("Файл: %1\nНазвание: %2\nИсполнитель: %3\nАльбом: %4\nЖанр: %5\nГод: %6\nДлительность: %7\nФормат: %8")
            .arg(media.fileName(), availableText(media.title()), availableText(media.artist()),
                 availableText(media.album()), availableText(media.genre()),
                 media.year() ? QString::number(media.year()) : QStringLiteral("—"),
                 durationText(media.length()), availableText(QFileInfo(media.fileName()).suffix().toUpper()));
    default:
        break;
    }

    if (role == SortRole) {
        if (index.column() == DurationColumn)
            return media.length();
        if (index.column() == FileNameColumn)
            return media.title().trimmed().isEmpty() ? media.fileName() : media.title();
    }
    if (role != Qt::DisplayRole && role != SortRole && role != Qt::AccessibleTextRole)
        return {};

    switch (index.column()) {
    // Keep this raw identity: legacy actions use the file name to address disk files.
    // The delegate obtains the human-readable title from TitleRole instead.
    case FileNameColumn: return media.fileName();
    case TitleColumn: return availableText(media.title());
    case ArtistColumn: return availableText(media.artist());
    case AlbumColumn: return availableText(media.album());
    case DurationColumn: return durationText(media.length());
    case FormatColumn: return availableText(QFileInfo(media.fileName()).suffix().toUpper());
    default: return {};
    }
}

int MediaModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !mMediaManager_ ? 0 : mMediaManager_->mediaCount();
}

int MediaModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

Qt::ItemFlags MediaModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;
    if (index.model() != this || index.row() < 0 || index.row() >= rowCount()
        || index.column() < 0 || index.column() >= ColumnCount)
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
}

void MediaModel::beginCollect()
{
    beginResetModel();
}

void MediaModel::endCollect()
{
    endResetModel();
}

void MediaModel::setManagerType(CHANNEL_TYPE type)
{
    if (mManagerType == type)
        return;
    mManagerType = type;
    if (rowCount())
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1), {MediaTypeRole});
}

CHANNEL_TYPE MediaModel::type()
{
    return mManagerType;
}

Qt::DropActions MediaModel::supportedDropActions() const
{
    return Qt::CopyAction;
}

QStringList MediaModel::mimeTypes() const
{
    return {QStringLiteral("text/uri-list")};
}

bool MediaModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
                               int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);
    if (!data || (action != Qt::CopyAction && action != Qt::IgnoreAction))
        return false;
    for (const QUrl &url : data->urls()) {
        if (url.isLocalFile() && !url.toLocalFile().isEmpty())
            return true;
    }
    return false;
}

bool MediaModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                            int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;
    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    QStringList files;
    for (const QUrl &url : data->urls()) {
        if (url.isLocalFile() && !url.toLocalFile().isEmpty())
            files.append(url.toLocalFile());
    }
    files.removeDuplicates();
    emit dropFileList(files);
    return true;
}
