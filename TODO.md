# QwenIRC — TODO / Requirements Checklist

Requirements are written in EARS (Easy Approach to Requirements Syntax) format.
Each item includes a user story and links to the audit finding.

Legend: `[ ]` = open · `[x]` = done · priority **P1** (critical) → **P4** (low)

---

## 1. IRC Protocol Correctness

### 1.1 CAP Negotiation

**REQ-CAP-01** · P1
> When the TCP connection is established, the system shall send `CAP LS 302` before sending `NICK` or `USER`, and shall not send `NICK`/`USER` until `CAP END` has been issued.

*User story:* As a user, I want the client to complete capability negotiation before registering, so that the server does not reject or ignore capability requests.

- [ ] Move `NICK`/`USER` to after `CAP END` in `NetworkManager::onConnected`
- [ ] Populate `m_capSupported` with at least `["sasl", "server-time", "echo-message", "multi-prefix", "away-notify", "account-notify"]`
- [ ] Parse the server's `CAP LS` token list and intersect with supported caps before sending `CAP REQ`
- [ ] Fix `handleCapCommand` LS branch — current loop captures cap names and discards them

**REQ-CAP-02** · P2
> When the server sends `CAP ACK`, the system shall enable each acknowledged capability and then send `CAP END`.

- [ ] Track acknowledged caps in a `QSet<QString> m_activeCaps`
- [ ] Activate `echo-message` flag to suppress local echo in `sendUserInput`

---

### 1.2 Numeric Reply Parameter Indexing

**REQ-NUM-01** · P1
> The system shall parse all numeric reply parameters with the understanding that parameter 0 is the recipient nick, and data begins at parameter 1 (or later), as specified in RFC 2812 §5.

*User story:* As a user, I want channel topics, mode strings, ban lists, and error messages to display correctly, so that I have accurate information about channel state.

- [x] Fix 324 RPL_CHANNELMODEIS: channel = `params[0]`, mode = `params[1]`
- [x] Fix 332 RPL_TOPIC: channel = `params[0]`, topic = `params[1]`
- [x] Fix 333 RPL_TOPICWHOTIME: channel = `params[0]`, setter = `params[2]`, ts = `params[1]`
- [x] Fix 329 RPL_CREATIONTIME: channel = `params[0]`, ts = `params[1]`
- [x] Fix 367 RPL_BANLIST: indexing corrected
- [x] Fix 368 RPL_ENDOFBANLIST: indexing corrected
- [x] Fix 401 ERR_NOSUCHNICK: missing nick = `params[1]`
- [x] Fix 331 RPL_NOTOPIC: channel = `params[0]`
- [x] Fix 315 RPL_ENDOFWHO: does not emit `namesComplete` — this is end-of-WHO
- [x] Fix error range 421-499: extract message from `params[1]`
- [x] Fix 433 retry limit (> 3, not >= 3; resets after error)
- [x] Add 005 ISUPPORT parsing (stores CHANTYPES, PREFIX, NETWORK, CASEMAPPING in `m_isupport`)
- [x] Use `m_isupport["CHANTYPES"]` for channel-name detection instead of hardcoded `'#'`

---

### 1.3 PRIVMSG / NOTICE Routing

**REQ-MSG-01** · P1
> When a `PRIVMSG` is received whose target is the client's own nickname, the system shall route the message to a query tab named after the sending user, creating the tab if it does not exist.

*User story:* As a user, I want private messages to open in a dedicated query tab automatically, so that I do not miss incoming PMs.

- [ ] Wire `handlePrivMsg` to call `MainWindow::addQueryTab` when `targetNick == m_nick`
- [ ] Add a `queryTabNeeded(const QString& nick)` signal to `NetworkManager`
- [ ] Connect `queryTabNeeded` → `MainWindow::addQueryTab` in `MainWindow::initializeUI`
- [ ] Apply same fix to ACTION CTCP when private (target is own nick → route to sender's query tab)

**REQ-MSG-02** · P2
> When a `NOTICE` is received whose target is the client's own nickname, the system shall route it to a query tab named after the sender, creating the tab if it does not exist.

- [ ] Apply the same PM-routing fix to `handleNotice`

**REQ-MSG-03** · P2
> While the `echo-message` capability is active, the system shall not emit a local echo in `sendUserInput` for channel messages.

- [ ] Guard the local echo block in `sendUserInput` with `!m_activeCaps.contains("echo-message")`

---

### 1.4 CTCP

**REQ-CTCP-01** · P2
> When a CTCP VERSION request is received, the system shall send a CTCP VERSION reply via `NOTICE` to the *requesting user's nick*, not to the channel or the client's own nick.

*User story:* As a user, I want CTCP replies to be routed correctly, so that other clients receive version information.

- [ ] Change `sendCtcpVersionReply(chanName)` → `sendCtcpVersionReply(senderNick)` in `handlePrivMsg` and `handleNotice`

**REQ-CTCP-02** · P2
> The system shall send a CTCP ACTION (`/me`) command when the user types `/me <text>` in a channel or query tab.

*User story:* As a user, I want to use `/me` to send action messages, so that I can express actions in IRC conversation.

- [ ] Add `/me` handler in `sendUserInput`: send `PRIVMSG <context> :\001ACTION <text>\001`

---

### 1.5 NICK and QUIT Propagation

**REQ-NICK-01** · P2
> When a `NICK` message is received, the system shall emit a nick-change notification only to tabs for channels in which the renaming user is currently a member.

*User story:* As a user, I want nick-change messages to appear only in relevant channel tabs, so that unrelated tabs are not cluttered.

- [ ] In `handleNick`, check `ch->findUser(oldNick) != nullptr` before emitting to that channel
- [ ] Update the user's entry in each `IRCChannel::m_users` list
- [ ] Update `IRCUserModel` in the corresponding `ChannelTab` (add `userNickChanged` signal or call removeUser/addUser)

**REQ-QUIT-01** · P2
> When a `QUIT` message is received, the system shall emit a quit notification to every channel the quitting user was a member of, remove the user from each channel's user list, and not emit to channels where the user was not present.

*User story:* As a user, I want quit messages to appear only where that user was, and the user list to reflect their departure, so that the list stays accurate.

- [ ] In `handleQuit`, iterate `m_channels`, check membership, emit and remove only where user was present
- [ ] Emit `userLeft` per channel (not just `m_currentChannel`)

---

### 1.6 KICK

**REQ-KICK-01** · P2
> When a `KICK` message is received, the system shall display which user was kicked, by whom, and the reason; the `IRCMessage` sender field shall be the kicker's nick.

- [ ] Fix `handleKick`: set sender to `kicker`, include `kicked` and `reason` in the message text

---

### 1.7 MODE

**REQ-MODE-01** · P2
> When a `MODE` message is received with parameters, the system shall store per-user mode changes (op, voice, etc.) against the correct user in the channel's user list, using the parameter list to identify the target user.

*User story:* As a user, I want user mode prefixes (`@`, `+`) in the user list to update correctly when ops are set or removed, so that I can see who has privileges.

- [ ] Rewrite `IRCChannel::applyMode(modeStr, params)` to accept the full parameter list
- [ ] Parse mode string character-by-character consuming params for user/address modes
- [ ] Update matching `IRCUser::setMode` and reflect change in `IRCUserModel`

---

### 1.8 NAMES Reply

**REQ-NAMES-01** · P2
> When a 353 RPL_NAMREPLY is received, the system shall preserve prefix characters (`@`, `+`, `~`, `%`, `&`) on user display in the user list.

*User story:* As a user, I want to see operator and voice prefixes next to nicks in the user list, so that I can identify privileged users at a glance.

- [ ] `onNamesReceived` currently adds only `user.nick()` to `m_userList` without prefix — include mode prefix

---

### 1.9 433 Nick In Use

**REQ-433-01** · P3
> When a 433 ERR_NICKNAMEINUSE reply is received, the system shall attempt at most 3 alternative nicks, then present the user with an error dialog.

- [ ] Add `int m_nickRetries` counter; after 3 attempts, emit `serverError` and stop

---

### 1.10 IRC Line Parsing

**REQ-PARSE-01** · P2
> When a received line contains a leading `\r`, the system shall strip the `\r` character, not replace it with a space.

- [x] Changed `line.replace('\r', QChar(' '))` → `line.remove('\r')`

**REQ-PARSE-02** · P2
> When a received line contains IRCv3 message tags (prefixed with `@`), the system shall parse and store relevant tag values (`server-time`, `account`, `msgid`) before dispatching the message.

*User story:* As a user, I want messages to display their original server timestamp (from `server-time` tag) when available, so that the time shown matches when the message was actually sent.

- [x] Parse `server-time` tag and set `IRCMessage::m_timestamp` from it
- [x] Added `IRCMessage::setTimestamp(const QDateTime&)` setter

**REQ-PARSE-03** · P2
> When the receive buffer overflows, the system shall discard the oldest data, preserving the most recent.

- [x] Changed `m_lineBuffer.truncate(MAX_BUFFER_SIZE / 2)` → `m_lineBuffer.right(m_lineBuffer.size() / 2)` — keeps newest half, not first half

**REQ-PARSE-04** · P3
> The system shall decode incoming bytes using UTF-8, and fall back to Latin-1 for sequences that are not valid UTF-8.

- [x] Attempts `QString::fromUtf8(rawLine)` first; if replacement chars (U+FFFD) appear, falls back to `QString::fromLatin1(rawLine)`

---

### 1.11 PING/PONG Keep-alive

**REQ-PING-01** · P3
> While connected, the system shall detect server silence exceeding 4 minutes and emit a disconnection event.

*User story:* As a user, I want the client to detect dead connections automatically, so that I am not left staring at a frozen chat.

- [ ] Start a `m_pongTimer` (4 min) on each PING sent; reset it on any incoming data; fire `serverError` + `disconnectFromHost()` on timeout

---

### 1.12 Channel Prefix / ISUPPORT

**REQ-005-01** · P3
> When a 005 RPL_ISUPPORT reply is received, the system shall parse and store `CHANTYPES`, `PREFIX`, `CASEMAPPING`, and `NETWORK` tokens.

*User story:* As a user, I want the client to respect server-specific channel types and nick prefixes, so that non-standard networks work correctly.

- [x] Parse `005` params into `QMap<QString, QString> m_isupport` (extracts key=value tokens)
- [x] Use `m_isupport["CHANTYPES"]` for channel-name detection in `sendUserInput`, `handleNotice`, and `handleNamesReceived`
- [ ] Use `PREFIX` for parsing NAMES mode chars in RPL_NAMREPLY

---

### 1.13 CAP — Empty CAP REQ

**REQ-CAP-03** · P1
> The system shall not send a `CAP REQ` with an empty capability list.

- [ ] Guard `CAP REQ` in `onConnected` — send only if cap list is non-empty

---

## 2. Security

### 2.1 TLS

**REQ-TLS-01** · P1
> Where the user selects a port ≥ 6697 or enables a TLS toggle, the system shall use `QSslSocket` for the connection.

*User story:* As a user, I want to connect to IRC over TLS, so that my password and messages are encrypted in transit.

- [ ] Replace `QTcpSocket` with `QSslSocket` in `NetworkManager`
- [ ] Add a TLS checkbox to `ServerDialog`
- [ ] Save TLS preference in `QSettings`

### 2.2 HTML Injection

**REQ-SEC-01** · P1
> The system shall HTML-escape all user-controlled strings (sender nick, message text, topic) before inserting them into HTML-formatted chat output.

*User story:* As a user, I want my chat display to not be hijacked by maliciously crafted nicks or messages, so that the UI renders safely.

- [ ] Apply `IRCMessage::escapeHTML()` (or `QString::toHtmlEscaped()`) to `m_sender` in `coloredText()`
- [ ] Apply HTML-escaping to all strings passed to `formattedText()` before embedding in `<span>` tags
- [ ] Replace hand-rolled `escapeHTML` with Qt's built-in `QString::toHtmlEscaped()`

### 2.3 Password Handling

**REQ-SEC-02** · P3
> The system shall not store the server password in `QSettings`.

- [ ] Confirm password is already excluded from settings save (it is — but document this intent)

---

## 3. Connection Management

### 3.1 Disconnect Double-Emission

**REQ-CONN-01** · P2
> When the user initiates a disconnect, the system shall emit `disconnected()` exactly once, after the socket fully closes.

- [ ] Remove manual `emit disconnected()` from `NetworkManager::disconnect()`; rely on `onDisconnected` slot

### 3.2 Reconnection

**REQ-CONN-02** · P3
> When the connection is lost unexpectedly, the system shall offer the user an option to reconnect with exponential backoff (max 5 attempts, 30 s cap).

*User story:* As a user, I want the client to attempt reconnection automatically, so that transient network drops don't require manual reconnection.

- [ ] Add reconnection state machine to `NetworkManager`

---

## 4. User Interface

### 4.1 Private Messages — Query Tab

**REQ-UI-PM-01** · P1
> When the client receives a private message and no query tab for that sender exists, the system shall create a query tab named after the sender and route the message to it.

*(See REQ-MSG-01 above — wiring work is in MainWindow)*

- [ ] `MainWindow` must listen for `queryTabNeeded` signal and call `addQueryTab`
- [ ] `addQueryTab` currently inserts at index 0; should append after existing tabs
- [ ] Query tabs need a `(PM)` indicator or distinct tab style

### 4.2 Chat Row Height

**REQ-UI-ROW-01** · P1
> The system shall render each chat message row at a height sufficient to display all wrapped text.

*User story:* As a user, I want long messages to wrap and display fully, so that I do not miss truncated content.

- [ ] `ChatItemDelegate::sizeHint` must compute document height using a `QTextDocument` with correct width, not return a hardcoded 18
- [ ] Cache `QTextDocument` per index/content to avoid re-parsing on every paint call

### 4.3 Auto-scroll Behaviour

**REQ-UI-SCROLL-01** · P2
> When new messages arrive, the system shall automatically scroll to the bottom only if the user's viewport was already at the bottom.

*User story:* As a user, I want to scroll up to read history without being snapped back to the bottom on each new message.

- [ ] In `ChatWidget::addMessage`, check `scrollBar->value() >= scrollBar->maximum() - 4` before calling `scrollToBottom()`

### 4.4 Message Copy

**REQ-UI-COPY-01** · P2
> The system shall allow the user to select and copy text from the chat view.

*User story:* As a user, I want to copy messages from the chat window, so that I can share or reference them.

- [ ] Enable text selection in `ChatWidget` (consider switching from `QListView` + delegate to `QTextBrowser` for rich HTML display)

### 4.5 User List Accuracy

**REQ-UI-UL-01** · P2
> When the currently visible channel tab changes, the system shall populate the user list with the users for that channel, regardless of whether the user list panel was visible when the NAMES reply arrived.

- [ ] On tab switch, repopulate user list from the in-memory `IRCChannel::m_users` rather than relying on the original NAMES signal timing

### 4.6 Channel Sidebar on Part

**REQ-UI-SIDEBAR-01** · P2
> When a channel tab is removed, the system shall also remove that channel's entry from the channel sidebar list.

- [ ] Add `m_channelList->takeItem(...)` in `MainWindow::removeChannelTab`

### 4.7 Channel Sidebar Double-Click

**REQ-UI-SIDEBAR-02** · P3
> When the user double-clicks an already-joined channel in the sidebar, the system shall switch to that channel's tab rather than issuing a JOIN.

- [ ] Change double-click handler to call `m_channelTabs->setCurrentWidget(findChannelTab(name))`

### 4.8 Nick Completion

**REQ-UI-NICK-01** · P4
> When the user presses Tab in the message input with a partial nick, the system shall complete to the nearest matching nick in the current channel.

*User story:* As a user, I want Tab-completion for nicks, so that I can address users quickly without typing full nicks.

- [ ] Implement tab-completion in `ChannelTab` / `m_inputEdit` key event filter

### 4.9 Input History

**REQ-UI-HIST-01** · P4
> The system shall allow the user to navigate previously sent messages using the Up/Down arrow keys in the input field.

- [ ] Add `QStringList m_inputHistory` with index to `ChannelTab`

### 4.10 mIRC Color Rendering

**REQ-UI-COLOR-01** · P4
> When a message contains mIRC color codes (`\x03N,M`), the system shall render the text in the corresponding foreground and background colors.

- [ ] Add mIRC color-code parser that converts `\x03N[,M]text` to `<span style="color:...">` in `IRCMessage::coloredText`

---

## 5. Architecture / Code Quality

### 5.1 Model Usage

**REQ-ARCH-01** · P2
> The system shall use `IRCUserModel` and `IRCChannelModel` as the data sources for the user list and channel sidebar views respectively, rather than imperative `QListWidget` item manipulation.

- [ ] Replace `m_userList` (`QListWidget`) with `QListView` + `IRCUserModel`
- [ ] Replace `m_channelList` (`QListWidget`) with `QListView` + `IRCChannelModel`
- [ ] Remove the dead `IRCChannelModel` created in `MainWindow::initializeUI` that is never set on any view

### 5.2 Display Logic in Model

**REQ-ARCH-02** · P3
> The `IRCMessage` class shall not contain HTML formatting logic; formatting shall be the responsibility of the view layer.

- [ ] Move `coloredText()` and `formattedText()` to a `IRCMessageFormatter` helper or the delegate
- [ ] Remove the duplicate color table in `IRCMessageModel::data` / `ColorRole`

### 5.3 Duplicate MAX_MESSAGES

**REQ-ARCH-03** · P3
> A single constant shall define the maximum number of retained messages per channel.

- [ ] Define `MAX_MESSAGES` once (e.g., in `IRCMessage.h` or a `constants.h`) and remove the duplicate in `IRCChannel.h`

### 5.4 m_userSet Dead Code

**REQ-ARCH-04** · P3
> `IRCChannel` shall use `m_userSet` for O(1) membership checks in `findUser`, `addUser`, and `removeUser`, or the field shall be removed.

- [ ] Fix `addUser` to check `m_userSet.contains(nick)` before inserting
- [ ] Fix `findUser` and `removeUser` to use the set
- [ ] Remove linear-scan duplicates

### 5.5 Duplicate findChannelTab / findQueryTab

**REQ-ARCH-05** · P3
> The system shall have a single `findTab(const QString& name)` function in `MainWindow`.

- [ ] Merge `findChannelTab` and `findQueryTab` into one method

### 5.6 Unused NetworkManager Argument in ChannelTab

**REQ-ARCH-06** · P4
> `ChannelTab` shall not accept a `NetworkManager*` parameter if it does not use it.

- [ ] Remove `NetworkManager* nm` from `ChannelTab` constructor signature and all call sites

### 5.7 Case-Insensitive Nick/Channel Comparison

**REQ-ARCH-07** · P2
> The system shall compare nick and channel names using the casemapping declared in `ISUPPORT` (default: `rfc1459`).

- [ ] Add `CaseMap` helper that normalises strings per the active casemapping
- [ ] Replace all `QString::operator==` comparisons for nicks and channels with the normalised form

### 5.8 IRCChannelModel::addChannel Notification

**REQ-ARCH-08** · P3
> `IRCChannelModel::addChannel` shall use `beginInsertRows`/`endInsertRows` rather than `beginResetModel`/`endResetModel`.

- [ ] Replace reset-model pair with proper insert-rows pair

### 5.9 IRCChannelModel::setCurrentChannel Empty Guard

**REQ-ARCH-09** · P3
> `IRCChannelModel::setCurrentChannel` shall not emit `dataChanged` when the channel list is empty.

- [ ] Add `if (m_channels.isEmpty()) return;` before the `emit dataChanged` call

### 5.10 Header Guard Name

**REQ-ARCH-10** · P4
> Each header file's include guard shall match the file name.

- [ ] Rename guard `IRCMODELMANAGER_H` → `IRCMESSAGEMODEL_H` in `IRCMessageModel.h`

---

## 6. Build / Infrastructure

### 6.1 Install Target

**REQ-BUILD-01** · P3
> The CMake install target shall install the executable to `bin/` on Linux and Windows in addition to the macOS bundle destination.

- [ ] Add `RUNTIME DESTINATION bin` to `install(TARGETS qwenirc ...)` in `CMakeLists.txt`

### 6.2 build.sh Portability

**REQ-BUILD-02** · P3
> `build.sh` shall locate the Qt6 CMake prefix using `qmake6 -query QT_INSTALL_PREFIX` or `cmake --find-package`, which are portable across distributions.

- [ ] Replace `qt6-cmake-config-path` with `$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || echo "/usr/lib/cmake/Qt6")`

### 6.3 C++ Extensions

**REQ-BUILD-03** · P4
> The build shall disable compiler-specific C++ extensions.

- [ ] Add `set(CMAKE_CXX_EXTENSIONS OFF)` to `CMakeLists.txt`

### 6.4 Tests

**REQ-TEST-01** · P3
> The project shall include unit tests for `NetworkManager::parseMessage`, `IRCMessage::coloredText`, and `IRCUserModel::addUser`/`removeUser`.

*User story:* As a developer, I want automated tests for the parser and models, so that regressions in protocol handling are caught before release.

- [ ] Add `find_package(Qt6 ... Test)` and a `tests/` directory with at least 3 test cases per class above

---

## 7. ServerDialog

- [ ] **P1** Instantiate `m_themeCombo` or remove the dead member declaration
- [ ] **P2** Replace `&QWidget::close` with `&QDialog::reject` for the Cancel button
- [ ] **P2** Add port number validator (`QIntValidator(1, 65535)`) to `m_portEdit`
- [ ] **P2** Add a TLS/SSL checkbox (see REQ-TLS-01)
- [ ] **P3** Save the channel field to `QSettings`
- [ ] **P3** Parse comma-separated channels in the channel field and join each after MOTD

---

## 8. Deferred / Out of Scope for v0.1

- [ ] SASL PLAIN / EXTERNAL authentication
- [ ] DCC file transfer
- [ ] Reconnection with backoff (stub exists, logic missing)
- [ ] Per-network configuration profiles
- [ ] Logging messages to disk
- [ ] Notification / highlight system
- [ ] Ignore list
