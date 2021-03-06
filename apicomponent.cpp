#include "apicomponent.h"

#include <QDomDocument>

ApiComponent::ApiComponent(QObject *parent) : QObject(parent)
{
    initializeGenresMap();
}

void ApiComponent::setOAuthTokens(const ApiComponent::OAuthTokensMap &tokens)
{
    tokens_ = tokens;
}

const ApiComponent::OAuthTokensMap& ApiComponent::tokens() const
{
    return tokens_;
}

const ApiComponent::GenresMap &ApiComponent::genres() const
{
    return genres_;
}

void ApiComponent::getTokensFromUrl(const QUrl& url)
{
    QString const urlString = url.toString();

    if (urlString.contains("error"))
        emit authorizeFinished(false, urlString.section("error_description=", 1));
    else if(urlString.contains("access_token"))
    {
        //!TODO =([^&]*)&?
        // ([\w]*)=([^&]*)&? <value, key>
        int const s_access = urlString.indexOf('=', 0) + 1;
        int const s_expires_in = urlString.indexOf('=', s_access) + 1;
        int const s_userid = urlString.indexOf('=', s_expires_in) + 1;
        int const e_access = urlString.indexOf('&', 0) + 1;
        int const e_expires_in = urlString.indexOf('&', e_access) + 1;
        int const e_userid = -1;
        tokens_[AccessToken] = urlString.mid(s_access, e_access - s_access - 1);
        tokens_[ExpiresIn] = urlString.mid(s_expires_in, e_expires_in - s_expires_in - 1);
        tokens_[UserId] = urlString.mid(s_userid, e_userid);

        emit authorizeFinished(true, QString());
    }
}

void ApiComponent::getPlaylistFromReply(QNetworkReply *reply)
{
    QDomDocument domDocument;
    domDocument.setContent(reply->readAll());

    QDomElement responseElement = domDocument.firstChildElement(); //! <response list = true>
    QDomNode itemNode = responseElement.firstChildElement(); //! <audio>

    Playlist playlist;

    while(!itemNode.isNull())
    {
        PlaylistItem data;
        QString const artist = itemNode.toElement().elementsByTagName("artist").item(0).toElement().text();
        QString const title = itemNode.toElement().elementsByTagName("title").item(0).toElement().text();
        QString const duration = itemNode.toElement().elementsByTagName("duration").item(0).toElement().text();
        QString const url = itemNode.toElement().elementsByTagName("url").item(0).toElement().text();

        if (!artist.isEmpty() && !title.isEmpty() && !duration.isEmpty() && !url.isEmpty())
        {
            data[Artist] = artist;
            data[Title] = title;
            data[Duration] = duration;
            data[Url] = url;

            playlist.push_back(data);
        }

        itemNode = itemNode.nextSibling();
    }

    reply->deleteLater();

    emit playlistReceived(playlist);
}

void ApiComponent::initializeGenresMap()
{
    genres_["Rock"] = Rock;
    genres_["Pop"] = Pop;
    genres_["Rap & Hip-hop"] = RapAndHipHop;
    genres_["Easy Listening"] = EasyListening;
    genres_["Dance & House"] = DanceAndHouse;
    genres_["Instrumental"] = Instrumental;
    genres_["Metal"] = Metal;
    genres_["Alternative"] = Alternative;
    genres_["Dubstep"] = Dubstep;
    genres_["Jazz & Blues"] = JazzAndBlues;
    genres_["Drum & Bass"] = DrumAndBass;
    genres_["Trance"] = Trance;
    genres_["Chanson"] = Chanson;
    genres_["Ethnic"] = Ethnic;
    genres_["Acoustic & Vocal"] = AcousticAndVocal;
    genres_["Reggae"] = Reggae;
    genres_["Classical"] = Classical;
    genres_["Indie Pop"] = IndiePop;
    genres_["Speech"] = Speech;
    genres_["Electropop & Disco"] = ElectropopAndDisco;
    genres_["Other"] = Other;
}

void ApiComponent::sendPlaylistRequest(const QString &request)
{
    QNetworkAccessManager * networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &ApiComponent::getPlaylistFromReply);
    QNetworkRequest networkRequest(request);

    QNetworkReply *reply = networkManager->get(networkRequest);
}

void ApiComponent::requestAuthUserPlaylist()
{
    Q_ASSERT(tokens_.contains(UserId));

    sendPlaylistRequest("https://api.vk.com/method/audio.get.xml?uid=" + tokens_[UserId]
                        + "&access_token=" + tokens_[AccessToken]);

}

void ApiComponent::requestSuggestedPlaylist()
{
    sendPlaylistRequest("https://api.vk.com/method/audio.getRecommendations.xml?uid=" +
                        tokens_[UserId] + "&access_token=" + tokens_[AccessToken] + "&count=500");
}

void ApiComponent::requestPopularPlaylistByGenre(const QString &genre)
{
    sendPlaylistRequest("https://api.vk.com/method/audio.getPopular.xml?uid=" +
                        tokens_[UserId] + "&access_token=" + tokens_[AccessToken] +
                        + "&genre_id=" + QString::number(genres_[genre]) + "&count=500");
}

void ApiComponent::requestPlaylistBySearchQuery(const ApiComponent::SearchQuery &query)
{
    sendPlaylistRequest("https://api.vk.com/method/audio.search.xml?uid=" +
                        tokens_[UserId] + "&access_token=" + tokens_[AccessToken] +
                        "&performer_only=" + QString::number(query.artist) +
                        "&q=" + query.text + "&count=300");
}
