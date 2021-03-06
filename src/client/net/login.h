/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef DP_CLIENT_NET_LOGINHANDLER_H
#define DP_CLIENT_NET_LOGINHANDLER_H

#include "../shared/net/message.h"

#include <QString>
#include <QUrl>
#include <QObject>
#include <QPointer>
#include <QMessageBox>
#include <QSslError>

namespace dialogs {
	class SelectSessionDialog;
	class LoginDialog;
}

namespace net {

class TcpServer;
class LoginSessionModel;

/**
 * @brief Login process state machine
 *
 * See also LoginHandler in src/shared/server/ for the serverside implementation
 */
class LoginHandler : public QObject {
	Q_OBJECT
public:
	enum Mode {HOST, JOIN};

	LoginHandler(Mode mode, const QUrl &url, QWidget *parent=0);

	/**
	 * @brief Set the desired user ID
	 *
	 * Only for host mode. When joining an existing session, the server assigns the user ID.
	 *
	 * @param userid
	 */
	void setUserId(int userid) { Q_ASSERT(_mode==HOST); _userid=userid; }

	/**
	 * @brief Set desired session ID
	 *
	 * Only in host mode. Use URL path when joining.
	 * @param id
	 */
	void setSessionId(const QString &id) { Q_ASSERT(_mode==HOST); _hostSessionId=id; }

	/**
	 * @brief Set the session password
	 *
	 * Only for host mode.
	 *
	 * @param password
	 */
	void setPassword(const QString &password) { Q_ASSERT(_mode==HOST); _sessionPassword=password; }

	/**
	 * @brief Set the session title
	 *
	 * Only for host mode.
	 *
	 * @param title
	 */
	void setTitle(const QString &title) { Q_ASSERT(_mode==HOST); _title=title; }

	/**
	 * @brief Set the maximum number of users the session will accept
	 *
	 * Only for host mode.
	 *
	 * @param maxusers
	 */
	void setMaxUsers(int maxusers) { Q_ASSERT(_mode==HOST); _maxusers = maxusers; }

	/**
	 * @brief Set whether new users should be locked by default
	 *
	 * Only for host mode.
	 *
	 * @param allowdrawing
	 */
	void setAllowDrawing(bool allowdrawing) { Q_ASSERT(_mode==HOST); _allowdrawing = allowdrawing; }

	/**
	 * @brief Set whether layer controls should be locked to operators only by default
	 *
	 * Only for host mode.
	 *
	 * @param layerlock
	 */
	void setLayerControlLock(bool layerlock) { Q_ASSERT(_mode==HOST); _layerctrllock = layerlock; }

	/**
	 * @brief Set whether the session should be persistent
	 *
	 * Only for host mode. Whether this option actually gets set depends on whether the server
	 * supports persistent sessions.
	 *
	 * @param persistent
	 */
	void setPersistentSessions(bool persistent) { Q_ASSERT(_mode==HOST); _requestPersistent = persistent; }

	/**
	 * @brief Set whether chat history should be preserved in the session
	 */
	void setPreserveChat(bool preserve) { Q_ASSERT(_mode==HOST); _preserveChat = preserve; }

	/**
	 * @brief Set session announcement URL
	 */
	void setAnnounceUrl(const QString &url) { _announceUrl = url; }

	/**
	 * @brief Set the server we're communicating with
	 * @param server
	 */
	void setServer(TcpServer *server) { _server = server; }

	/**
	 * @brief Handle a received message
	 * @param message
	 */
	void receiveMessage(protocol::MessagePtr message);

	/**
	 * @brief Login mode (host or join)
	 * @return
	 */
	Mode mode() const { return _mode; }

	/**
	 * @brief Server URL
	 * @return
	 */
	const QUrl &url() const { return _address; }

	/**
	 * @brief get the user ID assigned by the server
	 * @return user id
	 */
	int userId() const { return _userid; }

	/**
	 * @brief get the ID of the session.
	 *
	 * This is set to the actual session ID after login succeeds.
	 * @return session ID
	 */
	QString sessionId() const;

public slots:
	void serverDisconnected();

private slots:
	void joinSelectedSession(const QString &id, bool needPassword);
	void selectIdentity(const QString &password, const QString &username);
	void cancelLogin();
	void failLogin(const QString &message, const QString &errorcode=QString());
	void passwordSet(const QString &password);
	void tlsStarted();
	void tlsError(const QList<QSslError> &errors);
	void tlsAccepted();

private:
	enum State {
		EXPECT_HELLO,
		EXPECT_STARTTLS,
		WAIT_FOR_LOGIN_PASSWORD,
		EXPECT_IDENTIFIED,
		EXPECT_SESSIONLIST_TO_JOIN,
		EXPECT_SESSIONLIST_TO_HOST,
		WAIT_FOR_JOIN_PASSWORD,
		WAIT_FOR_HOST_PASSWORD,
		EXPECT_LOGIN_OK,
		ABORT_LOGIN
	};

	void expectNothing(const QString &msg);
	void expectHello(const QString &msg);
	void expectStartTls(const QString &msg);
	void prepareToSendIdentity();
	void sendIdentity();
	void expectIdentified(const QString &msg);
	void showPasswordDialog(const QString &title, const QString &text);
	void expectSessionDescriptionHost(const QString &msg);
	void sendHostCommand();
	void expectSessionDescriptionJoin(const QString &msg);
	void sendJoinCommand();
	void expectNoErrors(const QString &msg);
	void expectLoginOk(const QString &msg);
	void startTls();
	void send(const QString &message);
	void handleError(const QString &msg);

	Mode _mode;
	QUrl _address;
	QWidget *_widgetParent;

	// session properties for hosting
	int _userid;
	QString _sessionPassword;
	QString _title;
	int _maxusers;
	bool _allowdrawing;
	bool _layerctrllock;
	bool _requestPersistent;
	bool _preserveChat;
	QString _announceUrl;

	// Process state
	TcpServer *_server;
	State _state;
	LoginSessionModel *_sessions;

	QString _hostPassword;
	QString _joinPassword;
	QString _hostSessionId;
	QString _selectedId;
	QString _loggedInSessionId;

	QString _autoJoinId;

	QPointer<dialogs::SelectSessionDialog> _selectorDialog;
	QPointer<dialogs::LoginDialog> _passwordDialog;
	QPointer<QMessageBox> _certDialog;

	// Server flags
	bool _multisession;
	bool _tls;
	bool _canAuth;
	bool _mustAuth;
	bool _needUserPassword;
	bool _needHostPassword;
};

}

#endif
