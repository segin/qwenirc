# Qwen Execution Prompts for QwenIRC TODO

Paste one phase at a time. For each phase, paste the prompt **followed immediately
by the raw content of every file listed under "Attach these files"**.
Apply all output files before starting the next phase.

---

## Phase 1 — P1 Critical: CAP Negotiation, PM Routing, Disconnect, TLS Socket

**Attach these files (paste their full content after the prompt):**
- `CMakeLists.txt`
- `backend/NetworkManager.h`
- `backend/NetworkManager.cpp`
- `frontend/MainWindow.h`
- `frontend/MainWindow.cpp`
- `frontend/ServerDialog.h`
- `frontend/ServerDialog.cpp`

---

```
You are an expert Qt6/C++ developer. You will receive the source files of a Qt6
IRC client called QwenIRC. Rewrite the listed files to fix the issues below.
Output each file in full, delimited by a markdown code block whose info string is
the file path (e.g. ```cpp backend/NetworkManager.h). Do not truncate any file.
Do not add features beyond what is listed. Do not add comments unless the WHY is
non-obvious.

=== FIXES TO IMPLEMENT ===

--- backend/NetworkManager.h ---
1. Add `QSslSocket` include and change `m_socket` from `QTcpSocket*` to
   `QSslSocket*`.
2. Add `bool m_useTls = false;` member.
3. Add `QSet<QString> m_activeCaps;` member to track ACK'd capabilities.
4. Add `QMap<QString, QString> m_isupport;` member.
5. Add `void queryTabNeeded(const QString& nick);` signal.
6. Change `State state()` to `State state() const`.

--- backend/NetworkManager.cpp ---
7.  CAP negotiation order (REQ-CAP-01):
    In `onConnected`, send `CAP LS 302\r\n` FIRST, then PASS (if set).
    Do NOT send NICK or USER yet — move those into a new private slot
    `void sendRegistration()`.
    Start m_pingTimer only after registration completes (call it from
    sendRegistration or after 001 numeric).

8.  Populate capability list (REQ-CAP-01, REQ-CAP-03):
    Replace the empty `m_capSupported` construction with:
    `m_capSupported = {"server-time", "echo-message", "multi-prefix",
                       "away-notify", "account-notify"};`
    In handleCapCommand LS: parse the server's advertised caps, intersect with
    m_capSupported, and only REQ the intersection (so the CAP REQ list is never
    empty). If the intersection is empty, call sendRegistration() immediately
    and send CAP END.
    In handleCapCommand ACK: store each ack'd cap in m_activeCaps, then send
    CAP END, then call sendRegistration().
    In handleCapCommand NAK: send CAP END, then call sendRegistration().

9.  sendRegistration() implementation:
    Sends NICK, then USER <nick> 8 * :<nick>. This is the only place those are
    sent.

10. Fix disconnect() double-emission (REQ-CONN-01):
    Remove the manual `emit disconnected()` and the state assignment from
    `disconnect()`. Let `onDisconnected()` handle it. Keep
    `m_socket->disconnectFromHost()` and the stateChanged emission of
    Disconnected state.

11. Fix PM routing (REQ-MSG-01):
    In handlePrivMsg, when `targetNick == m_nick`, emit
    `queryTabNeeded(senderNick)` before emitting channelMessage. Keep
    routing the channelMessage to senderNick as the channel key.
    Apply same pattern to private CTCP ACTION.

12. Fix CTCP VERSION reply target (REQ-CTCP-01):
    Change `sendCtcpVersionReply(chanName)` to
    `sendCtcpVersionReply(senderNick)` in both handlePrivMsg and handleNotice.

13. Fix buffer overflow direction (REQ-PARSE-03):
    Replace `m_lineBuffer.truncate(MAX_BUFFER_SIZE / 2)` with
    `m_lineBuffer = m_lineBuffer.mid(m_lineBuffer.size() / 2);`

14. Fix \r handling (REQ-PARSE-01):
    Change `line.replace('\r', QChar(' '))` to `line.remove('\r')`.

15. Fix 433 retry limit (REQ-433-01):
    Add `int m_nickRetries = 0;`. In the 433 handler, if m_nickRetries < 3,
    append "_", increment counter, call setNick(m_nick+"_"). Otherwise emit
    serverError("Nickname in use — please reconnect with a different nick.")
    and call disconnect(). Reset m_nickRetries to 0 in sendRegistration().

16. Fix onPingTimeout not detecting dead connection:
    Add `QTimer* m_pongTimer;` (4-minute timeout). In onPingTimeout, start
    m_pongTimer. In onReadyRead (top, before buffering), call
    m_pongTimer->stop() if it is active. In m_pongTimer timeout slot, emit
    serverError("Server timeout") then call disconnect(). Initialize and wire
    m_pongTimer in constructor.

17. echo-message guard (REQ-MSG-03):
    In sendUserInput, guard the local echo block with:
    `if (!m_activeCaps.contains("echo-message")) { ... }`

18. TLS support (REQ-TLS-01):
    Add `void connectToServer(..., bool useTls)` overload (or add `bool useTls`
    param to existing signature). Store in m_useTls. In onConnected (or just
    before connectToHost), if m_useTls, use QSslSocket::connectToHostEncrypted.
    Add `find_package(Qt6 REQUIRED COMPONENTS ... NetworkAuth)` is NOT needed —
    QSslSocket is in Qt6::Network. Add error handling for sslErrors signal
    (ignore self-signed for now; emit serverError with the description).

--- frontend/MainWindow.h ---
19. Add `void onQueryTabNeeded(const QString& nick);` slot.

--- frontend/MainWindow.cpp ---
20. Wire queryTabNeeded signal in initializeUI:
    `connect(m_network, &NetworkManager::queryTabNeeded,
             this, &MainWindow::onQueryTabNeeded);`

21. Implement onQueryTabNeeded:
    Call addQueryTab(nick) — this already exists.

22. Fix ServerDialog cancel button (REQ-UI-DIALOG-01):
    The call site in showConnectionDialog is fine; the fix is in ServerDialog.

--- frontend/ServerDialog.h ---
23. Declare `QCheckBox* m_tlsCheck;` member (add QCheckBox include).

--- frontend/ServerDialog.cpp ---
24. Add QCheckBox include.
25. Instantiate m_tlsCheck with label "Use TLS/SSL", add to layout row 2
    (shift port and remaining rows down by one), default checked = false.
26. Save/restore m_tlsCheck state to QSettings key "tls".
27. Replace `connect(m_cancelBtn, ..., &QWidget::close)` with
    `connect(m_cancelBtn, ..., this, &QDialog::reject)`.
28. Add `bool useTls() const { return m_tlsCheck->isChecked(); }` accessor.
29. In applyConnection emit, add useTls() to connectRequested signal.
    Update signal signature in ServerDialog.h: add `bool useTls` parameter.
30. In MainWindow::onConnect, forward useTls to m_network->connectToServer.

--- CMakeLists.txt ---
31. Add `find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Ssl)` — 
    actually Qt6::Network includes SSL; just add `Qt6::Network` target_link_libraries
    is already there. No change needed unless on a system needing explicit SSL.
    Add `RUNTIME DESTINATION bin` to install target.
    Add `set(CMAKE_CXX_EXTENSIONS OFF)`.

=== END FIXES ===
```

---

## Phase 2 — P1 Security + UI: HTML Injection, Chat Row Height, Auto-scroll, Copy

**Attach these files:**
- `backend/IRCMessage.h`
- `backend/IRCMessage.cpp`
- `frontend/ChatWidget.h`
- `frontend/ChatWidget.cpp`

---

```
You are an expert Qt6/C++ developer working on QwenIRC, a Qt6 IRC client.
Rewrite the listed files to fix the issues below. Output each file in full in a
markdown code block whose info string is the file path. Do not truncate. Do not
add features beyond what is listed.

=== FIXES TO IMPLEMENT ===

--- backend/IRCMessage.h ---
1.  Add `void setTimestamp(const QDateTime& dt) { m_timestamp = dt; }` setter.
2.  Remove the declaration of `escapeHTML` — replace usages with
    `QString::toHtmlEscaped()`.

--- backend/IRCMessage.cpp ---
3.  HTML injection (REQ-SEC-01):
    In coloredText(), every string that comes from the network (m_sender,
    m_text, and the return value of formattedText()) MUST be passed through
    `QString::toHtmlEscaped()` before being placed inside a <span> or used in
    a format string that goes into HTML. Specifically:
    - For MessageType::Message: escape m_sender and m_text separately (m_text
      is already escaped via the old escapeHTML call — switch it to
      toHtmlEscaped()).
    - For all other types: escape the result of formattedText() before wrapping
      in <span>.
    Delete the static escapeHTML method body and declaration entirely. Replace
    all call sites with QString::toHtmlEscaped().

4.  Fix formattedText() TopicSet placeholder (line ~37):
    `QString("Topic: %2").arg(m_text)` → `QString("Topic: %1").arg(m_text)`

5.  Fix formattedText() Part branch: the `if (parts.size() >= 1)` branch
    produces the same output as the else; simplify to just:
    `result = QString("%1 left %2").arg(m_sender, m_text);`
    Remove the dead else branch.

6.  Fix formattedText() Kick: include kicker (m_sender) and reason (m_text):
    `result = QString("%1 was kicked: %2").arg(m_sender, m_text);`
    (The caller sets sender to the channel name currently — that is fixed in
    Phase 1 NetworkManager changes. Here just use m_sender as-is.)

7.  Remove the unused `#include <QRegularExpression>`.

--- frontend/ChatWidget.h ---
8.  No structural changes needed.

--- frontend/ChatWidget.cpp ---
9.  Fix row height (REQ-UI-ROW-01):
    Replace ChatItemDelegate::sizeHint to compute the actual document height:

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override {
        QString text = index.data(Qt::DisplayRole).toString();
        QTextDocument doc;
        doc.setHtml(text);
        doc.setTextWidth(option.rect.width() > 0 ? option.rect.width() : 600);
        return QSize(qRound(doc.idealWidth()),
                     qRound(doc.size().height()) + 2);
    }

10. Fix paint to match (the translate and clipRect already use rect.top() and
    rect.size() — make sure they're consistent with the dynamic height):
    In paint(), remove `doc.setPageSize(QSize(..., 10000))`. After
    `doc.setTextWidth(option.rect.width())`, just call
    `doc.drawContents(painter, QRectF(QPointF(0,0), option.rect.size()))`.
    Keep `painter->translate(0, rect.top())` before draw.

11. Fix auto-scroll (REQ-UI-SCROLL-01):
    In ChatWidget::scrollToBottom(), only scroll if user is at/near bottom:
    Replace the body with:
        QScrollBar* sb = m_chatList->verticalScrollBar();
        if (sb && sb->value() >= sb->maximum() - 4)
            sb->setValue(sb->maximum());

12. Enable text selection/copy (REQ-UI-COPY-01):
    On m_chatList, change:
        setSelectionMode(QAbstractItemView::NoSelection)
    to:
        setSelectionMode(QAbstractItemView::ExtendedSelection)
    And change:
        setFocusPolicy(Qt::NoFocus)
    to:
        setFocusPolicy(Qt::StrongFocus)

13. Fix runtime type check (REQ-ARCH):
    In addMessage() and clearMessages(), replace:
        m_chatModel->inherits("IRCMessageModel")
        static_cast<IRCMessageModel*>(m_chatModel)
    with:
        qobject_cast<IRCMessageModel*>(m_chatModel)
    and check the result for nullptr.

14. Remove duplicate setChannel/setChannelName — keep only setChannelName,
    have setChannel call setChannelName, or just remove setChannel entirely
    since all callers use setChannelName.

=== END FIXES ===
```

---

## Phase 3 — P2 Protocol: Numeric Replies, NICK/QUIT Propagation, MODE, NAMES

**Attach these files (use the post-Phase-1 versions):**
- `backend/NetworkManager.h`
- `backend/NetworkManager.cpp`

---

```
You are an expert Qt6/C++ developer working on QwenIRC, a Qt6 IRC client.
You will receive updated NetworkManager files (already fixed in Phase 1).
Apply the additional fixes below. Output the full updated files.

=== FIXES TO IMPLEMENT ===

--- Numeric reply parameter indexing (REQ-NUM-01) ---
In ALL numeric handlers, remember: params[0] = recipient nick (skip it).
Data starts at params[1] unless noted.

1.  324 RPL_CHANNELMODEIS:
    channel = params.value(1), mode = params.value(2)
    (Currently reads params[0] as channel, params[1] as mode — wrong.)

2.  332 RPL_TOPIC:
    channel = params.value(1), topic = params.value(2)
    Emit channelTopic(channel, topic).
    (Currently reads params[0] as channel, params[1] as topic — wrong.)

3.  333 RPL_TOPICWHOTIME:
    channel = params.value(1), setter = params.value(2), ts = params.value(3)

4.  329 RPL_CREATIONTIME:
    channel = params.value(1), ts = params.value(2)

5.  367 RPL_BANLIST and 368 RPL_ENDOFBANLIST:
    Shift all params.value(N) by +1.

6.  401 ERR_NOSUCHNICK:
    missing nick = params.value(1) (not params.value(0))

7.  331 RPL_NOTOPIC:
    channel = params.value(1)

8.  Error range 421-499:
    message = params.value(2, params.value(1))
    (Try params[2] first, fall back to params[1] for shorter replies.)

9.  Fix 315 RPL_ENDOFWHO:
    Do NOT emit namesComplete here — 315 is end-of-WHO, not end-of-NAMES.
    Just emit serverChannelMessage.

--- NICK propagation (REQ-NICK-01) ---
10. In handleNick, only emit channelMessage to channels where the renaming
    user is currently a member. Also update the user's entry in that channel:
    call ch->removeUser(oldNick), then ch->addUser(IRCUser(newNick)).
    After the loop, emit userChangedNick as before.

--- QUIT propagation (REQ-QUIT-01) ---
11. In handleQuit, iterate m_channels. For each channel:
    - Check if ch->findUser(nick) != nullptr
    - If yes: ch->removeUser(nick), emit channelMessage to that channel,
      emit userLeft(it.key(), nick, reason)
    Remove the single emit userLeft(m_currentChannel, ...) line.

--- MODE parameters (REQ-MODE-01) ---
12. In handleMode, pass the full params list to a new method
    `void applyUserMode(IRCChannel* ch, const QStringList& params)`:
    Parse params[1] (the mode string) character by character, maintaining a
    param index starting at 2. For each letter in the mode string:
    - If '+' or '-', update the add/remove flag.
    - If 'o' or 'v' or 'h' or 'a' or 'q': consume next param as the target
      nick. Find that IRCUser in ch, update their mode field accordingly.
    - All other mode chars: consume a param if the mode is +/- and the char
      is in "beIklf" (common param-taking modes); otherwise no param.
    This replaces the broken IRCChannel::applyMode. Keep the existing
    IRCMessage emission for display.

--- CTCP /me private (REQ-MSG-01) ---
13. In handlePrivMsg, for CTCP ACTION when targetNick == m_nick:
    Route the action message to senderNick tab (not to chanName/m_nick).

--- sendUserInput /me handler (REQ-CTCP-02) ---
14. Add a `/me` command handler in sendUserInput:
    else if (cmd == "ME") {
        if (!context.isEmpty() && context != "Server" && parts.size() >= 2) {
            QString actionText = parts.mid(1).join(' ');
            sendRaw("PRIVMSG " + context + " :\001ACTION " + actionText
                    + "\001\r\n");
            if (!m_activeCaps.contains("echo-message")) {
                IRCMessage msg(MessageType::Message,
                               "*" + actionText + "*", m_nick);
                msg.setChannel(context);
                emit channelMessage(context, msg);
            }
        }
    }

--- ISUPPORT 005 parsing (REQ-005-01) ---
15. In the 005 handler, parse each token in params.mid(1) of the form KEY=VALUE
    (or bare KEY) and store in m_isupport:
    for (const QString& token : params.mid(1)) {
        if (token.startsWith(':')) continue;
        int eq = token.indexOf('=');
        if (eq > 0) m_isupport[token.left(eq)] = token.mid(eq+1);
        else m_isupport[token] = "1";
    }
    Use m_isupport.value("CHANTYPES", "#&+!") when checking if a target is a
    channel (replace all `target.startsWith('#')` guards with a helper:
    `bool isChannel(const QString& t) const { return
    m_isupport.value("CHANTYPES","#&+!").contains(t.isEmpty()?QChar():t[0]); }`

=== END FIXES ===
```

---

## Phase 4 — P2 Architecture: Models, IRCChannel, MainWindow UX, ServerDialog

**Attach these files (use post-Phase-1/3 versions where applicable):**
- `backend/IRCChannel.h`
- `backend/IRCChannel.cpp`
- `backend/IRCMessageModel.h`
- `backend/IRCMessageModel.cpp`
- `frontend/MainWindow.h`
- `frontend/MainWindow.cpp`
- `frontend/ChannelTab.h`
- `frontend/ChannelTab.cpp`
- `frontend/ServerDialog.h`
- `frontend/ServerDialog.cpp`

---

```
You are an expert Qt6/C++ developer working on QwenIRC, a Qt6 IRC client.
Apply the fixes below to the attached files. Output every file in full.

=== FIXES TO IMPLEMENT ===

--- backend/IRCChannel.h / IRCChannel.cpp ---
1.  Fix addUser duplicate check (REQ-ARCH-04):
    In addUser(), guard with `if (m_userSet.contains(user.nick())) return;`
    before inserting into m_users.

2.  Fix findUser to use the set first (O(1) fast path):
    Keep the linear scan for returning a pointer, but guard the loop:
    `if (!m_userSet.contains(nick)) return nullptr;`

3.  Remove applyMode(const QString&) entirely — it is replaced by the
    per-user mode logic in NetworkManager (Phase 3). Keep the method signature
    in the header but make the body empty with a [[deprecated]] attribute, or
    just remove it if no other callers exist.

4.  Remove MAX_MESSAGES from IRCChannel — use a single constant. Add to
    IRCMessage.h: `inline constexpr int kMaxMessages = 10000;`
    and reference it in both IRCChannel.cpp and IRCMessageModel.cpp.

--- backend/IRCMessageModel.h / IRCMessageModel.cpp ---
5.  Fix header guard: change `IRCMODELMANAGER_H` to `IRCMESSAGEMODEL_H`.

6.  Fix IRCChannelModel::addChannel to use beginInsertRows/endInsertRows:
    int row = m_channels.size();
    beginInsertRows(QModelIndex(), row, row);
    m_channels.append(name);
    endInsertRows();

7.  Fix IRCChannelModel::setCurrentChannel empty-list guard:
    if (!m_channels.isEmpty())
        emit dataChanged(index(0,0), index(m_channels.size()-1, 0));

8.  Fix IRCMessageModel::insertSystemMessage to use MessageType::Notice
    instead of MessageType::TopicSet (which is a display type, not a system
    message type). This is a semantic correction only.

--- frontend/MainWindow.h / MainWindow.cpp ---
9.  Replace the two QListWidget members (m_channelList, m_userList) with
    QListView + model backing:
    - Replace `QListWidget* m_channelList` with `QListView* m_channelList`
    - Replace `QListWidget* m_userList` with `QListView* m_userList`
    - The existing m_channelModel (IRCChannelModel*) should be set on
      m_channelList via setModel().
    - Add `IRCUserModel* m_userModel` member; create it in initializeUI;
      set on m_userList via setModel().
    Update all imperative addItem/takeItem/clear/count/item() calls on both
    list widgets to use the corresponding model methods instead:
    - m_channelList item add → m_channelModel->addChannel(name)
    - m_channelList item remove → m_channelModel->removeChannel(name)
    - m_userList population → m_userModel->setUsers(...) or addUser/removeUser
    - m_userList clear → m_userModel->clear()
    Update the dead `connect(m_network, &NetworkManager::userReceived, ...)`
    lambda to use m_userModel instead of m_userList.

10. Fix removeChannelTab to also remove from channel model:
    Add `m_channelModel->removeChannel(name);` inside removeChannelTab.

11. Fix onNamesReceived to always update user model regardless of visibility:
    Remove the `if (m_userList->isVisible())` guard. Always call
    `m_userModel->setUsers(users)` when channel == m_currentChannel.
    Also include mode prefix in display (IRCUserModel::data already does this
    — verify it shows mode + nick).

12. Fix onUserChangedNick to post to all channel tabs, not just current:
    Iterate m_channelTabs (all tabs), find ChannelTab* via qobject_cast, call
    addMessage on each with the nick-change message.
    Also update m_userModel: removeUser(oldNick), addUser(IRCUser(newNick)).

13. Fix onUserJoined to use model: replace m_userList->addItem with
    m_userModel->addUser(user).

14. Fix onUserLeft to use model: replace linear search + takeItem with
    m_userModel->removeUser(nick).

15. Fix tab-change handler to repopulate user model from IRCChannel data:
    In the currentChanged lambda, after setting m_currentChannel, call:
    IRCChannel* ch = m_network->channel(m_currentChannel);
    if (ch) m_userModel->setUsers(ch->users());
    else m_userModel->clear();

16. Fix channel sidebar double-click to switch tabs, not rejoin:
    Replace `m_network->joinChannel(name)` with:
    ChannelTab* tab = findChannelTab(name);
    if (tab) m_channelTabs->setCurrentWidget(tab);

17. Merge findChannelTab and findQueryTab into one private method:
    `ChannelTab* findTab(const QString& name)` that searches all tabs.
    Update all callers.

18. Remove unused m_serverInfo member.

--- frontend/ChannelTab.h / ChannelTab.cpp ---
19. Remove NetworkManager* nm parameter from constructor entirely.
    Update the header declaration and all callers in MainWindow.cpp
    (pass no NetworkManager argument).

--- frontend/ServerDialog.h / ServerDialog.cpp ---
20. Remove the dead `m_themeCombo` declaration and any null member.
    (m_themeCombo was declared but never instantiated — just delete it.)

21. Add port validator in constructor:
    m_portEdit->setValidator(new QIntValidator(1, 65535, this));

22. Save and restore channel field in QSettings.

=== END FIXES ===
```

---

## Notes for Qwen

- Work on one phase at a time. Do not anticipate changes from a later phase.
- Where a fix references a method that does not yet exist (e.g., from a
  prior phase), implement a stub or note the dependency.
- Preserve all existing signal/slot names unless the fix explicitly renames one.
- Do not add logging, comments, documentation strings, or README updates.
- Do not add `#pragma once` — this project uses `#ifndef` guards.
- If a file is not listed in a phase, do not output it.
