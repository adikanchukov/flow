#include "playerwidget.h"
#include "ui_playerwidget.h"

#include <QFileDialog>
#include <QButtonGroup>
#include <QMenu>
#include <QMessageBox>
#include <QMediaPlaylist>

PlayerWidget::PlayerWidget(MediaComponent *media, ApiComponent *api, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PlayerWidget),
    api_(api), media_(media),
    model_(new QStandardItemModel(this)), playlist_(new QMediaPlaylist(this)),
    stillCurrentPlaylist_(false)
{
    Q_ASSERT(media);
    Q_ASSERT(api);

    ui->setupUi(this);

    ui->musicSubMenuListWidget->clearSelection();

    QActionGroup *playbackGroup = new QActionGroup(this);
    playbackGroup->addAction("Shuffle");
    playbackGroup->addAction("Repeat Off");
    playbackGroup->addAction("Repeat Single");
    playbackGroup->addAction("Repeat All");

    foreach(QAction *action, playbackGroup->actions())
        action->setCheckable(true);
    playbackGroup->actions().at(1)->setChecked(true);

    QMenu *menu = new QMenu(ui->playbackButton);
    menu->addActions(playbackGroup->actions());
    ui->playbackButton->setMenu(menu);

    connect(playbackGroup, &QActionGroup::triggered, this, &PlayerWidget::playbackModeChanged);

    ui->playlistTableView->setModel(model_);
    ui->playlistTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->playlistTableView->horizontalHeader()->setVisible(false);

    connect(api_, &ApiComponent::playlistReceived, this, &PlayerWidget::setPlaylist);
    connect(this, &PlayerWidget::playlistCleared, media_, &MediaComponent::clearPlaylist);
    connect(this, &PlayerWidget::playlistItemAdded, media_, &MediaComponent::addItemToPlaylist);

    connect(ui->searchEdit, &QLineEdit::returnPressed, this, &PlayerWidget::search);
    connect(ui->searchButton, &QPushButton::clicked, this, &PlayerWidget::search);

    connect(this, &PlayerWidget::requestedPopularByGenre, api_, &ApiComponent::requestPopularPlaylistByGenre);

    connect(ui->volumeSlider, &QSlider::sliderMoved, this, &PlayerWidget::changeVolume);
    connect(ui->timeSlider, &QSlider::sliderMoved, this, &PlayerWidget::seek);

    connect(ui->playlistTableView, &QTableView::doubleClicked, this, &PlayerWidget::playIndex);
    connect(this, &PlayerWidget::startedPlaying, media_, &MediaComponent::playIndex);
    connect(media_->playlist(), &QMediaPlaylist::currentIndexChanged, this, &PlayerWidget::currentPlayItemChanged);
    connect(media_->player(), &QMediaPlayer::durationChanged, this, &PlayerWidget::durationChanged);
    connect(media_->player(), &QMediaPlayer::positionChanged, this, &PlayerWidget::positionChanged);
    connect(media_->player(), &QMediaPlayer::volumeChanged, this, &PlayerWidget::volumeChanged);
    connect(media_->player(), &QMediaPlayer::stateChanged, this, &PlayerWidget::stateChanged);
}

PlayerWidget::~PlayerWidget()
{
    delete ui;
}

void PlayerWidget::setPlaylist(const ApiComponent::Playlist& playlist)
{
    stillCurrentPlaylist_ = false;

    clear();

    foreach (const ApiComponent::PlaylistItem& item, playlist)
        addItem(item);
}

void PlayerWidget::clear()
{
    model_->removeRows(0, model_->rowCount());
    playlist_->clear();
}

void PlayerWidget::addItem(const ApiComponent::PlaylistItem &item)
{
    int const rowCount = model_->rowCount();
    model_->insertRow(rowCount);

    model_->setItem(rowCount, ApiComponent::Artist, new QStandardItem(item[ApiComponent::Artist]));
    model_->setItem(rowCount, ApiComponent::Title, new QStandardItem(item[ApiComponent::Title]));
    QStandardItem* durationItem = new QStandardItem(convertSecondsToTimeString(item[ApiComponent::Duration].toInt()));
    durationItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    model_->setItem(rowCount, ApiComponent::Duration, durationItem);

    playlist_->addMedia(QUrl(item[ApiComponent::Url]));
}

QString PlayerWidget::convertSecondsToTimeString(int seconds)
{
    QTime time(seconds/3600, seconds/60, seconds % 60);
    QString const format = seconds > 3600 ? "hh:mm:ss" :"mm:ss";
    return time.toString(format);
}

void PlayerWidget::playIndex(const QModelIndex &index)
{
    if (!stillCurrentPlaylist_)
    {
        media_->copyModel(model_);
        media_->copyPlaylist(playlist_);
        stillCurrentPlaylist_ = true;
    }

    int const currentIndex = index.row();
    emit startedPlaying(currentIndex);
}

void PlayerWidget::setPlayItemStatusText(const QString& title, const QString& artist)
{
    ui->titleButton->setText(title);
    ui->artistButton->setText(artist);
    setWindowTitle(title + " by " + artist + " - " + "Flow");
}

void PlayerWidget::currentPlayItemChanged(int position)
{
    if (position == -1)
    {
        media_->pause();
        return;
    }

    QString const artist = media_->model()->item(position, ApiComponent::Artist)->text();
    QString const title = media_->model()->item(position, ApiComponent::Title)->text();

    if (stillCurrentPlaylist_)
        ui->playlistTableView->selectRow(position);

    setPlayItemStatusText(title, artist);
}

void PlayerWidget::playbackModeChanged(QAction *action)
{
    QString const playbackMode = action->text();
    if (playbackMode == "Shuffle")
        media_->setPlaybackMode(QMediaPlaylist::Random);
    if (playbackMode == "Repeat Single")
        media_->setPlaybackMode(QMediaPlaylist::CurrentItemInLoop);
    else if (playbackMode == "Repeat All")
        media_->setPlaybackMode(QMediaPlaylist::Loop);
    else if (playbackMode == "Repeat Off")
        media_->setPlaybackMode(QMediaPlaylist::Sequential);
}

void PlayerWidget::durationChanged(qint64 duration)
{
    ui->timeSlider->setMaximum(duration / 1000);
}

void PlayerWidget::positionChanged(qint64 progress)
{
    if (!ui->timeSlider->isSliderDown())
        ui->timeSlider->setValue(progress / 1000);

    updatePositionInfo(progress / 1000);
}

void PlayerWidget::updatePositionInfo(qint64 progress)
{
    int const estimatedDuration = media_->duration() - progress;
    QString const position = convertSecondsToTimeString(progress);
    QString const duration = convertSecondsToTimeString(estimatedDuration);

    ui->positionLabel->setText(position);
    ui->durationLabel->setText("-" + duration);
}

void PlayerWidget::seek(int seconds)
{
    media_->setPosition(seconds * 1000);
}

void PlayerWidget::volumeChanged(int value)
{
    if(!ui->volumeSlider->isSliderDown())
        ui->volumeSlider->setValue(value);
}

void PlayerWidget::changeVolume(int volume)
{
    media_->setVolume(volume);
}

void PlayerWidget::on_playPauseButton_clicked()
{
    QMediaPlayer::State const state = media_->state();
    if(state == QMediaPlayer::PlayingState)
        media_->pause();
    else if(state == QMediaPlayer::PausedState)
        media_->play();
    else
    {
        QModelIndexList const selectedIndexes = ui->playlistTableView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty())
            playIndex(selectedIndexes.at(0));
    }
}

void PlayerWidget::stateChanged(QMediaPlayer::State state)
{
    if(state == QMediaPlayer::PlayingState)
    {
        ui->playPauseButton->setIcon(QIcon(":/icons/pause.png"));
        ui->playPauseButton->setToolTip("Pause");
    }
    else if (state == QMediaPlayer::PausedState || state ==QMediaPlayer::StoppedState)
    {
        ui->playPauseButton->setIcon(QIcon(":/icons/play.png"));
        ui->playPauseButton->setToolTip("Play");
    }
}

void PlayerWidget::on_forwardButton_clicked()
{
    media_->next();
}

void PlayerWidget::on_rewindButton_clicked()
{
    media_->previous();
}

void PlayerWidget::search()
{   
    clearMusicMenusSelections();
    ui->searchButton->setChecked(true);
    QPushButton *sender = qobject_cast<QPushButton*>(QObject::sender());
    ApiComponent::SearchQuery query;
    query.artist = (sender && (sender == ui->artistButton)) ? true : false;
    query.text = ui->searchEdit->text();
    api_->requestPlaylistBySearchQuery(query);
}

void PlayerWidget::on_playlistButton_toggled(bool checked)
{
    if (checked)
    {
        clearMusicMenusSelections();
        stillCurrentPlaylist_ = true;
        ui->playlistTableView->setModel(media_->model());
        ui->playlistTableView->selectRow(media_->playlist()->currentIndex());
    }
    else
        ui->playlistTableView->setModel(model_);
}

void PlayerWidget::on_titleButton_clicked()
{
    ui->searchEdit->setText(ui->titleButton->text());
    search();
}

void PlayerWidget::on_artistButton_clicked()
{
    ui->searchEdit->setText(ui->artistButton->text());
    search();
}

void PlayerWidget::on_musicMenuListWidget_clicked(const QModelIndex &index)
{
    ui->playlistButton->setChecked(false);
    ui->searchButton->setChecked(false);

    const int row = index.row();

    if (row != PopularMusic)
        ui->musicSubMenuListWidget->setCurrentRow(-1);

    switch(index.row())
    {
    case MyMusic:
    {
        api_->requestAuthUserPlaylist();
        break;
    }
    case SuggestedMusic:
    {
        api_->requestSuggestedPlaylist();
        break;
    }
    case PopularMusic:
    {
        ui->musicSubMenuListWidget->setCurrentRow(0);
        api_->requestPopularPlaylistByGenre(ui->musicSubMenuListWidget->currentItem()->text());
        break;
    }
    default: break;
    }
}

void PlayerWidget::on_musicSubMenuListWidget_clicked(const QModelIndex &index)
{
    ui->playlistButton->setChecked(false);
    ui->searchButton->setChecked(false);

    ui->musicMenuListWidget->setCurrentRow(PopularMusic);
    api_->requestPopularPlaylistByGenre(ui->musicSubMenuListWidget->item(index.row())->text());
}

void PlayerWidget::clearMusicMenusSelections()
{
    ui->musicMenuListWidget->clearSelection();
    ui->musicSubMenuListWidget->clearSelection();
}
