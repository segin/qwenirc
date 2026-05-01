# QwenIRC — Audit Fix Tasklist (round 2)

Each task carries:

- **User story** — who needs the behaviour and why.
- **Requirements** in EARS form (`The system shall…`, `When X, the system shall…`, `If Y, then the system shall…`).
- **Location** — file, function, line range.
- **The bug** — current behaviour, with the offending code quoted.
- **Fix** — drop-in replacement code.
- **Acceptance** — observable evidence the requirement is met.

When a task is done and acceptance has been verified, mark its checkbox `[x]`.

---

## CRITICAL

### [x] B-1. Channel-mode display must not mutate the tab title

**User story.**
As an IRC user, I want my channel tabs to keep showing the channel name regardless of what mode information the server reports, so that I can identify and click into the right tab without the label shifting under me.

**Requirements (EARS).**
- **REQ-B1-1 (ubiquitous).** The system shall keep each channel tab's display text equal to the channel's IRC name for the entire lifetime of the tab.
- **REQ-B1-2 (event-driven).** When the system receives a channel-mode update, the system shall make the mode string available through the tab's tooltip.
- **REQ-B1-3 (unwanted behaviour).** If the channel-mode string is empty, then the system shall fall back to displaying just the channel name in the tooltip.

**Location.** `frontend/ChannelTab.cpp`, function `setMode`, lines ~60–66.

**The bug.**

```cpp
void ChannelTab::setMode(const QString& mode) {
    QTabWidget* tw = parentWidget() ? qobject_cast<QTabWidget*>(parentWidget()->parentWidget()) : nullptr;
    if (tw && !mode.isEmpty()) {
        tw->setTabText(tw->indexOf(this), channelName() + " [" + mode + "]");
    }
}
```

After the first `RPL 324` arrives, the tab's display text becomes `#chan [+nt]`. Every other site in `MainWindow` looks tabs up by `tabText`, so the mutated text breaks message routing (see B-2). Symptom reported by the user: PRIVMSGs from other people stop appearing in the channel tab.

**Fix.**

```cpp
void ChannelTab::setMode(const QString& mode) {
    QTabWidget* tw = parentWidget() ? qobject_cast<QTabWidget*>(parentWidget()->parentWidget()) : nullptr;
    if (!tw) return;
    int idx = tw->indexOf(this);
    if (idx < 0) return;
    tw->setTabToolTip(idx, mode.isEmpty() ? channelName() : channelName() + "  [" + mode + "]");
    // tab text stays as channelName() — do not rewrite it
}
```

**Acceptance.** `cmake --build build` clean; after joining a channel and receiving RPL 324, the tab text reads exactly `#qwenirc` (or whatever was joined), and hovering the tab shows the mode in a tooltip.

---

### [x] B-2. Tab routing must use the channel-name property, not the display text

**User story.**
As an IRC user, I want messages from other people in a channel to appear in that channel's tab, so that I can read and reply to the conversation.

**Requirements (EARS).**
- **REQ-B2-1 (ubiquitous).** The system shall identify a channel tab by the `ChannelTab::channelName()` property, never by the tab's user-visible display text.
- **REQ-B2-2 (event-driven).** When the system receives a `channelMessage` for an existing channel tab, the system shall append the message to that tab's chat view.
- **REQ-B2-3 (unwanted behaviour).** If no tab exists for the message's channel, then the system shall not crash, silently discard the message without logging, or post it to an unrelated tab.

**Location.** `frontend/MainWindow.cpp`. Five sites:

- `findChannelTab` — lines ~461–467
- `findQueryTab` — lines ~511–517
- `tabCloseRequested` lambda inside `initializeUI` — line ~82
- `currentChanged` lambda — lines ~187–196 (also covered by B-4)
- `removeChannelTab` loop — line ~447

**The bug.** Each site compares against `m_channelTabs->tabText(i)`. After B-1 mutates a tab's text, the comparison fails and `findChannelTab` returns `nullptr`. The PRIVMSG handler then has no tab to deliver the message into and the message is dropped.

**Fix.** Replace every `tabText`-based comparison with a `qobject_cast<ChannelTab*>(...)->channelName()` comparison.

`findChannelTab`:

```cpp
ChannelTab* MainWindow::findChannelTab(const QString& name) {
    for (int i = 0; i < m_channelTabs->count(); ++i) {
        auto* tab = qobject_cast<ChannelTab*>(m_channelTabs->widget(i));
        if (tab && tab->channelName() == name) return tab;
    }
    return nullptr;
}
```

`findQueryTab` — same body, or have it call `findChannelTab` and return the result.

`tabCloseRequested` lambda:

```cpp
connect(m_channelTabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
    auto* tab = qobject_cast<ChannelTab*>(m_channelTabs->widget(index));
    if (!tab || tab->channelName() == "Server") return;
    tab->close();
});
```

`removeChannelTab`:

```cpp
void MainWindow::removeChannelTab(const QString& name) {
    for (int i = 0; i < m_channelTabs->count(); ++i) {
        auto* tab = qobject_cast<ChannelTab*>(m_channelTabs->widget(i));
        if (tab && tab->channelName() == name) {
            m_channelTabs->removeTab(i);
            break;
        }
    }
    if (m_channelModels.contains(name)) {
        delete m_channelModels.take(name);
    }
}
```

`currentChanged` is rewritten in B-4.

**Acceptance.** After joining `#qwenirc` and waiting for RPL 324, an external user's PRIVMSG to that channel appears in the channel tab in real time.

---

## HIGH

### [x] B-3. Server-originated NOTICEs must land in the Server tab, not spawn a query tab

**User story.**
As an IRC user, I want pre-registration server messages (hostname lookup, ident lookup, services notices, network broadcasts) to appear in the Server tab, so that my tab bar isn't polluted with a tab named after the server hostname.

**Requirements (EARS).**
- **REQ-B3-1 (event-driven).** When the system receives a NOTICE whose source prefix contains neither `!` nor `@`, the system shall route the message to the Server tab.
- **REQ-B3-2 (event-driven).** When the system receives a NOTICE addressed to `*` or to `AUTH`, the system shall route the message to the Server tab regardless of source.
- **REQ-B3-3 (event-driven).** When the system receives a NOTICE from a user (prefix `nick!ident@host`) addressed to the local nick, the system shall route the message to a query tab for that user, creating the tab if absent.
- **REQ-B3-4 (unwanted behaviour).** If the source prefix is a server hostname, then the system shall not call `queryTabNeeded` for it.

**Location.** `backend/NetworkManager.cpp`, function `handleNotice`, lines ~486–522.

**The bug.**

```cpp
QString sender = prefix.section('!', 0, 0);
...
if (target.startsWith(chantype) && channel(target)) {
    emit channelMessage(target, msg);
} else {
    emit channelMessage(sender, msg);
    emit queryTabNeeded(sender);
}
```

A server prefix has no `!`, so `prefix.section('!', 0, 0)` returns the whole hostname. The else-branch fires `queryTabNeeded("weber.oftc.net")` and `MainWindow::addQueryTab` creates a tab labelled with the server hostname.

**Fix.** Replace the else-branch (keep the existing CTCP block above it unchanged):

```cpp
bool isServerSender = !prefix.contains('!') && !prefix.contains('@');
bool isPreRegTarget = (target == "*" || target == "AUTH");

QChar chantype = m_isupport["CHANTYPES"].isEmpty() ? QChar('#') : m_isupport["CHANTYPES"].front();
if (target.startsWith(chantype) && channel(target)) {
    emit channelMessage(target, msg);
} else if (isServerSender || isPreRegTarget) {
    emit serverChannelMessage(QString("[%1] %2").arg(sender).arg(text));
} else {
    emit channelMessage(sender, msg);
    emit queryTabNeeded(sender);
}
```

**Acceptance.** Connecting to `irc.oftc.net` produces no tab named after the server hostname; the "*** Looking up your hostname…" / "*** Found your hostname" / similar pre-registration notices appear in the Server tab.

---

### [x] B-4. Active-tab change must repopulate the user list from the new channel

**User story.**
As an IRC user, when I switch back to a channel tab I want the user-list sidebar to show that channel's users, so that I always see who is in the channel I'm reading.

**Requirements (EARS).**
- **REQ-B4-1 (event-driven).** When the system makes a channel tab active, the system shall populate the user-list sidebar with the users currently known for that channel.
- **REQ-B4-2 (event-driven).** When the system makes the Server tab active, the system shall hide the user-list sidebar and clear its contents.
- **REQ-B4-3 (event-driven).** When the system makes a query (private message) tab active, the system shall hide the user-list sidebar.
- **REQ-B4-4 (state-driven).** While a channel tab is active, the system shall display each user's mode prefix (`@`, `+`, etc.) prepended to the user's nick in the sidebar.

**Location.** `frontend/MainWindow.cpp`, the `currentChanged` lambda inside `initializeUI`, lines ~186–196.

**The bug.**

```cpp
connect(m_channelTabs, &QTabWidget::currentChanged, this, [this](int index) {
    QString tabName = m_channelTabs->tabText(index);
    if (tabName == "Server") {
        m_currentChannel = "";
        m_userList->clear();
        m_userList->setVisible(false);
    } else {
        m_currentChannel = tabName;
        m_userList->setVisible(true);
    }
});
```

The lambda only toggles user-list visibility. It never clears and repopulates from the channel that's now active, so switching from `#a` to `#b` shows `#a`'s users.

**Fix.** Look up the active tab via `ChannelTab::channelName()` (depends on B-2), then rebuild the list from `m_network->channel(name)->users()`:

```cpp
connect(m_channelTabs, &QTabWidget::currentChanged, this, [this](int index) {
    auto* tab = qobject_cast<ChannelTab*>(m_channelTabs->widget(index));
    QString name = tab ? tab->channelName() : QString();
    if (name.isEmpty() || name == "Server") {
        m_currentChannel.clear();
        m_userList->clear();
        m_userList->setVisible(false);
        return;
    }
    m_currentChannel = name;
    m_userList->clear();
    if (auto* ch = m_network->channel(name)) {
        for (const IRCUser& u : ch->users()) {
            QString prefix = u.userPrefix();
            m_userList->addItem(prefix.isEmpty() ? u.nick() : prefix + u.nick());
        }
    }
    m_userList->setVisible(true);
});
```

(Query tabs have no `IRCChannel`; the loop is skipped — correct for a PM.)

**Acceptance.** Join `#a` and `#b`, observe both sidebars while the tab is active. Switching tabs always shows the active channel's user list.

---

## CARRY-OVER (still open from the previous pass)

### [x] M-1 (finish). `handleQuit` must snapshot affected channels before iterating

**User story.**
As an IRC user, I want my client to remain stable when a user quits, so that a connected slot reacting to one of my channels can't crash the application.

**Requirements (EARS).**
- **REQ-M1-1 (event-driven).** When the system processes a QUIT, the system shall first compute the set of channels containing the quitting user and shall iterate that set when emitting per-channel signals.
- **REQ-M1-2 (unwanted behaviour).** If a slot connected to `channelMessage` modifies `m_channels` during iteration, then the system shall not invalidate any iterator currently in use.

**Location.** `backend/NetworkManager.cpp`, function `handleQuit`.

**The bug.** The handler still iterates `m_channels` directly while emitting `channelMessage` and calling `removeUser`, even though `handleNick` was rewritten to snapshot keys first. Same iterator-invalidation hazard.

**Fix.** Apply the same `keysToNotify` pattern used in `handleNick`: collect the keys for which `findUser(nick)` is non-null, then iterate the key list, performing emit/remove inside that second loop.

**Acceptance.** Build clean; existing tests pass; a QUIT broadcast for a user present in multiple channels updates each channel exactly once with no crash.

---

### [x] M-4 (finish). RPL 333 message must include the topic-set timestamp

**User story.**
As an IRC user, I want the channel-topic banner to show both who set the topic and when, so that I have full provenance for the topic I'm reading.

**Requirements (EARS).**
- **REQ-M4-1 (event-driven).** When the system receives RPL 333, the system shall emit a channel message containing the setter's nick and a human-readable rendering of the timestamp.

**Location.** `backend/NetworkManager.cpp`, numeric 333 branch in `handleNumericReply`.

**The bug.** The current branch reads `params.value(3)` into `timestamp` and then drops it. The displayed text is `"Topic set by X"`.

**Fix.** Compose the message with both fields:

```cpp
} else if (num == 333) {
    QString channel = params.value(1, "");
    QString setter  = params.value(2, "");
    QString tsStr   = params.value(3, "");
    QDateTime when = QDateTime::fromSecsSinceEpoch(tsStr.toLongLong());
    QString msgText = QString("Topic set by %1 on %2")
                          .arg(setter.section('!', 0, 0))
                          .arg(when.toString(Qt::ISODate));
    IRCMessage msg(MessageType::Topic, msgText, "");
    msg.setChannel(channel);
    emit channelMessage(channel, msg);
}
```

**Acceptance.** Joining a channel with a topic shows both lines in the channel tab — the topic itself (RPL 332) and "Topic set by X on YYYY-MM-DDThh:mm:ss" (RPL 333).

---

### [x] L-4 (still open). Mute the CAP-acknowledged / CAP-rejected announcements

**User story.**
As an IRC user, I don't want to see implementation chatter about which IRCv3 capabilities the server acknowledged or rejected, so that the Server tab is reserved for things I actually need to read.

**Requirements (EARS).**
- **REQ-L4-1 (event-driven).** When the system processes a `CAP ACK` or `CAP NAK`, the system shall not emit `serverChannelMessage`.
- **REQ-L4-2 (optional feature).** Where developer-debug logging is desired, the system shall log the acknowledged or rejected capabilities through `qDebug()`.

**Location.** `backend/NetworkManager.cpp`, in `handleCapCommand`, ACK and NAK branches.

**The bug.** Two `emit serverChannelMessage("Capabilities acknowledged: ...")` and `"Capabilities rejected"` calls remain.

**Fix.** Replace both with `qDebug()` (`#include <QDebug>` if needed) or delete them.

**Acceptance.** Connecting produces no "Capabilities acknowledged" / "Capabilities rejected" line in the Server tab.

---

### [x] L-7 (still open). Move `sendRaw` to `protected:` and remove `friend class TestIrcParser`

**User story.**
As a maintainer of `qwenirc`, I want test scaffolding to live in the test code rather than in production headers, so that production code is not coupled to test internals and the `friend` blast radius is removed.

**Requirements (EARS).**
- **REQ-L7-1 (ubiquitous).** The system shall expose `sendRaw` as a `protected` virtual method, accessible to subclasses but not to arbitrary callers.
- **REQ-L7-2 (ubiquitous).** The system header shall declare no `friend` relationships with test classes.

**Location.** `backend/NetworkManager.h`.

**The bug.** `virtual void sendRaw(const QString& data);` is in the `private:` section and `friend class TestIrcParser;` is at the bottom. Both exist purely so the test subclass in `tests/test_irc_parser.cpp` can override and capture sent commands.

**Fix.** Add a `protected:` section, move `sendRaw` into it, delete the friend declaration:

```cpp
protected:
    virtual void sendRaw(const QString& data);

private:
    // (rest of private section, with friend declaration removed)
```

The test subclass already inherits publicly and only needs `protected` access, so no changes are required in `tests/test_irc_parser.cpp`.

**Acceptance.** Production header has no `friend` directive; tests still compile and pass.

---

### [x] R-1 (regression). Reset or remove `m_hasSentCapLs`

**User story.**
As an IRC user, I want to be able to reconnect after a disconnect within the same session, so that I'm not forced to restart the application after a transient network blip.

**Requirements (EARS).**
- **REQ-R1-1 (event-driven).** When the system enters the connected state, the system shall send a `CAP LS 302` request to the server.
- **REQ-R1-2 (event-driven).** When the system disconnects, the system shall reset any internal flag that gates CAP negotiation, so that a subsequent reconnect re-runs CAP LS.
- **REQ-R1-3 (unwanted behaviour).** If the system reconnects after a previous successful CAP negotiation, then the system shall not skip CAP LS on the new connection.

**Location.** `backend/NetworkManager.h` (member declaration) and `backend/NetworkManager.cpp` (`onConnected`).

**The bug.**

```cpp
// NetworkManager.h:
bool m_hasSentCapLs = false;

// NetworkManager.cpp, onConnected:
if (!m_hasSentCapLs) {
    m_hasSentCapLs = true;
    sendRaw("CAP LS 302\r\n");
}
```

The flag is set on the first connect and never reset. After a disconnect/reconnect, the second `onConnected` skips CAP LS, the server sees no CAP request, `sendRegistration()` is never called from the CAP ACK path, and the client hangs unregistered.

The flag is also unnecessary: the plain-TCP `connected` signal and the SSL `encrypted` signal are wired to `onConnected` from disjoint code paths, and only one of them is wired up at a time per connection — they don't double-fire.

**Fix (preferred).** Delete the flag and restore the simple body:

```cpp
void NetworkManager::onConnected() {
    m_state = Connected;
    emit stateChanged(m_state);
    emit connected();
    sendRaw("CAP LS 302\r\n");
}
```

Remove `bool m_hasSentCapLs = false;` from the header.

**Fix (alternative).** If keeping the flag, reset it in `onDisconnected` and at the top of `connectToServer`.

**Acceptance.** `/quit` from a connected session, then reconnect via the File menu. The new connection completes CAP negotiation and registers; the client lands in a usable state.

---

## Verification (run after all fixes)

1. `cmake --build build --parallel` — builds clean with no warnings.
2. `ctest --output-on-failure` — all three test binaries pass.
3. Manual smoke against `irc.libera.chat:6667` or `irc.oftc.net:6667`:
   - Connecting produces no tab named after the server hostname; pre-registration NOTICEs land in the Server tab. (B-3)
   - The Server tab does not show "Capabilities acknowledged" or "Capabilities rejected". (L-4)
   - `/join #qwenirc` — tab text remains exactly `#qwenirc`; mode appears in the tab tooltip. (B-1)
   - Another user sends a message in the channel — the message appears in real time. (B-1, B-2)
   - Switch tabs and switch back — user-list sidebar reflects the active channel's users. (B-4)
   - Channel topic shows both the topic line and the "Topic set by X on …" line. (M-4)
   - `/quit` and reconnect from the File menu — connect completes and the client is usable. (R-1)
