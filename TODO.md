# QwenIRC — Audit Fix Tasklist

Each task below is one of the issues identified in the audit. Tasks are ordered by severity. For every task: read the file, find the location, understand what's wrong, then apply the fix exactly as described. When done, mark the checkbox `[x]`.

---

## CRITICAL

### [ ] C-1. Split CAP ACK capabilities by space

**File:** `backend/NetworkManager.cpp`, function `handleCapCommand`, lines ~740–760.

**The bug.** The IRC server sends `CAP ACK` like this:

```
:server CAP nick ACK :sasl server-time echo-message
```

After `parseMessage` runs, `params` contains `["nick", "ACK", "sasl server-time echo-message"]` — three elements, the third being a *single space-delimited string*. The current code does:

```cpp
QStringList caps = params.mid(2);   // ["sasl server-time echo-message"] — ONE element
for (const auto& cap : caps) {
    QString capName = cap;          // "sasl server-time echo-message"
    ...
    m_activeCaps.insert(capName);   // inserts the whole string as one cap
}
```

So `m_activeCaps` ends up containing the literal key `"sasl server-time echo-message"` instead of three separate keys `"sasl"`, `"server-time"`, `"echo-message"`.

**Why it matters.** Later in `sendUserInput`:

```cpp
if (!m_activeCaps.contains("echo-message")) {
    // emit local echo
}
```

This always returns `false` because the set contains the joined string, not `"echo-message"`. Result: every outgoing message is locally echoed even when the server is already echoing it back via `echo-message`, producing duplicate messages on screen.

**Fix.** Replace the `params.mid(2)` line so the trailing param is split into individual cap tokens:

```cpp
QStringList caps;
if (params.size() > 2) {
    caps = params[2].split(' ', Qt::SkipEmptyParts);
}
```

The rest of the loop body stays the same. Do the same fix for the `LS` and `NAK` branches if they have the same pattern.

---

### [ ] C-2. Initialise traffic-log pointers to nullptr

**File:** `backend/NetworkManager.h`, lines ~143–146.

**The bug.** The header declares:

```cpp
QFile* m_trafficLog;
QTextStream* m_trafficLogStream;
```

Neither has an in-class default initialiser, and neither is set in the `NetworkManager` constructor's initialiser list. In C++, raw pointer members without an initialiser hold *indeterminate values* — reading them is undefined behaviour.

**Why it matters.** `logTraffic()` does:

```cpp
if (!m_trafficLogStream) return;
```

This test reads an indeterminate pointer. On most platforms it happens to be non-null garbage, so the `return` doesn't fire and the next line dereferences the garbage pointer, crashing. `sendRaw()` calls `logTraffic` on every outgoing line, so the program would crash on the very first sent command.

**Fix.** Add `= nullptr` to both declarations in the header:

```cpp
QFile* m_trafficLog = nullptr;
QTextStream* m_trafficLogStream = nullptr;
```

---

## HIGH

### [ ] H-1. Fix the CTCP VERSION reply format

**File:** `backend/NetworkManager.cpp`, function `sendCtcpVersionReply`, line ~982.

**The bug.** Current code:

```cpp
QString reply = "\001VERSION\001 QwenIRC 0.1.0 \001";
```

A CTCP message is delimited by a single pair of `\001` bytes, with the command and arguments inside. The current string contains *three* `\001` bytes. Other clients parse the first `\001VERSION\001` as a complete (empty) CTCP VERSION reply and ignore everything after.

**Fix.** One pair of delimiters, command and version inside:

```cpp
QString reply = "\001VERSION QwenIRC 0.1.0\001";
```

---

### [ ] H-2. Don't reply to CTCP VERSION received in a NOTICE

**File:** `backend/NetworkManager.cpp`, function `handleNotice`, lines ~501–503.

**The bug.** Current code:

```cpp
if (ctcpCommand.toUpper() == "VERSION") {
    sendCtcpVersionReply(sender);  // WRONG
    emit ctcpReply(sender, "VERSION", ctcpText);
}
```

IRC convention: **CTCP requests are sent in `PRIVMSG`; CTCP replies are sent in `NOTICE`.** A CTCP VERSION inside a NOTICE is the *response* to our own earlier VERSION query. Replying to it sends another VERSION reply, which the peer's client (if it has the same bug) will reply to, and so on — a tight reply loop.

**Fix.** Remove the `sendCtcpVersionReply` call. Only emit the `ctcpReply` signal so UI can display the version string we received:

```cpp
if (ctcpCommand.toUpper() == "VERSION") {
    emit ctcpReply(sender, "VERSION", ctcpText);
}
```

The CTCP VERSION reply is correctly sent in `handlePrivMsg`, where it belongs.

---

### [ ] H-3. Handle `@` and `*` channel-visibility prefixes in RPL 353

**File:** `backend/NetworkManager.cpp`, in `handleNumericReply` for numeric 353, lines ~853–870.

**The bug.** RPL_NAMREPLY has the form `353 <client> <symbol> <channel> :<users>` where `<symbol>` is one of `=` (public channel), `@` (secret channel), or `*` (private channel). The current code only recognises `=`:

```cpp
QString channel = params.value(1, "");
int paramIdx = 2;
if (channel == "=") {
    channel = params.value(2, "");
    paramIdx = 3;
} else if (channel.startsWith('=')) {
    channel = channel.mid(1);
}
```

For a secret channel, `params[1]` is `"@"`, so `channel` ends up being `"@"`, `paramIdx` stays `2`, and we read the channel name `"#secret"` as the *user list*. NAMES gets routed to a phantom channel called `"@"` and the actual NAMES are silently dropped.

**Fix.** Extend the condition to all three symbols:

```cpp
QString channel = params.value(1, "");
int paramIdx = 2;
if (channel == "=" || channel == "@" || channel == "*") {
    channel = params.value(2, "");
    paramIdx = 3;
}
```

The `else if (channel.startsWith('='))` branch can be deleted — it's dead code with the corrected condition above (and it was wrong anyway, the `=` is always a separate token, not a prefix on the channel name).

---

### [ ] H-4. Preserve user mode prefix on nick change

**File:** `frontend/MainWindow.cpp`, function `onUserChangedNick`, lines ~423–458.

**The bug.** In `NetworkManager::handleNick` (NM.cpp ~525–556), the user is renamed in the channel's user list *before* the `userChangedNick` signal is emitted:

```cpp
ch->removeUser(oldNick);
ch->addUser(IRCUser(newNick, ident, host));
emit channelMessage(it.key(), msg);
// ... (loop continues for other channels)
```

`emit userChangedNick(oldNick, newNick)` then runs (after the loop, in the same function), and `MainWindow::onUserChangedNick` does:

```cpp
IRCUser* user = ch ? ch->findUser(oldNick) : nullptr;   // returns nullptr — already renamed!
...
if (user) {
    m_userList->item(i)->setText(user->userPrefix() + newNick);
} else {
    m_userList->item(i)->setText(newNick);              // prefix lost
}
```

`findUser(oldNick)` returns `nullptr` because the user is now stored as `newNick`. The sidebar entry is then updated to bare `newNick` — the `@`/`+`/etc. prefix is gone.

**Fix.** Look up the renamed user by `newNick` instead of `oldNick`:

```cpp
IRCUser* user = ch ? ch->findUser(newNick) : nullptr;
```

That's the only change needed. The `if (user)` branch already uses `user->userPrefix()` which is correct.

---

### [ ] H-5. Guard against empty `context` before indexing

**File:** `backend/NetworkManager.cpp`, function `sendUserInput`, line ~109.

**The bug.** Current code:

```cpp
} else if (cmd == "TOPIC") {
    if ((m_isupport["CHANTYPES"].isEmpty() ? '#' : m_isupport["CHANTYPES"].front().toLatin1()) == context[0]
            && context != "Server") {
```

`context[0]` is evaluated *before* the `&& context != "Server"` short-circuit can save us. If `context` is an empty string, `context[0]` is undefined behaviour in release builds (and asserts in debug). Empty `context` shouldn't happen via the UI path, but a `/topic` issued on a tab with an empty title or a future code path could trigger it.

**Fix.** Reorder so the empty check runs first:

```cpp
} else if (cmd == "TOPIC") {
    QChar chanType = m_isupport["CHANTYPES"].isEmpty() ? QChar('#') : m_isupport["CHANTYPES"].front();
    if (!context.isEmpty() && context != "Server" && context[0] == chanType) {
```

This also factors out the awkward double-ternary so the line is readable.

---

## MEDIUM

### [ ] M-1. Avoid iterator invalidation when iterating `m_channels`

**File:** `backend/NetworkManager.cpp`, functions `handleNick` (~545) and `handleQuit` (~675).

**The bug.** Both functions do:

```cpp
for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
    IRCChannel* ch = it.value();
    if (ch->findUser(nick) != nullptr) {
        ...
        emit channelMessage(it.key(), msg);   // synchronous slot call
    }
}
```

`emit` with a direct connection runs the connected slot synchronously *inside* `emit`. If any connected slot ever modified `m_channels` (e.g., a future `/part` triggered by a UI event during the message broadcast), the iterator would be invalidated and the next `++it` would crash. No connected slot does this today, but the pattern is fragile.

**Fix.** Snapshot the keys first, then iterate the snapshot:

```cpp
QList<QString> keysToNotify;
for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
    if (it.value()->findUser(nick) != nullptr) {
        keysToNotify.append(it.key());
    }
}
for (const QString& key : keysToNotify) {
    IRCChannel* ch = m_channels.value(key);
    if (!ch) continue;
    // ... apply rename / quit logic, emit signals ...
}
```

Apply the same restructuring to `handleQuit`.

---

### [ ] M-2. Parse IRCv3 message tags by first `=` only

**File:** `backend/NetworkManager.cpp`, function `parseLine`, lines ~316–322.

**The bug.** Current code:

```cpp
QStringList tagPairs = tags.split(';', Qt::SkipEmptyParts);
for (const QString& tag : tagPairs) {
    QStringList kv = tag.split('=');
    if (kv.size() == 2 && kv[0] == "time") {
        serverTime = kv[1];
    }
}
```

`tag.split('=')` splits on *every* `=`. The current `time` tag values (ISO 8601 timestamps) don't contain `=`, but other IRCv3 tags certainly do (`msgid`, `label`, vendor-prefixed extension tags). For any such tag, `kv.size()` is `>2`, the `==2` check fails, and the tag is silently skipped. Future tag-aware code will fail mysteriously.

**Fix.** Split on the first `=` only:

```cpp
QStringList tagPairs = tags.split(';', Qt::SkipEmptyParts);
for (const QString& tag : tagPairs) {
    int eq = tag.indexOf('=');
    QString key = (eq < 0) ? tag : tag.left(eq);
    QString value = (eq < 0) ? QString() : tag.mid(eq + 1);
    if (key == "time") {
        serverTime = value;
    }
}
```

---

### [ ] M-3. Don't put the channel name inside the Part message text

**File:** `backend/NetworkManager.cpp` line ~606 (handlePart), and `backend/IRCMessage.cpp` lines ~22–24 (formattedText for Part).

**The bug.** Current `handlePart`:

```cpp
IRCMessage msg(MessageType::Part, chanName + " " + reason, nick);
```

And `IRCMessage::formattedText` for Part:

```cpp
case MessageType::Part:
    result = (!m_text.isEmpty()) ? QString("%1 left %2").arg(m_sender).arg(m_text)
                                 : QString("%1 left").arg(m_sender);
```

The combined effect is `"alice left #channel because lunch"` where `#channel because lunch` is one blob. The channel name belongs in the message's channel field, not its text; the text should be just the reason.

**Fix in `NetworkManager.cpp`** — pass the reason as text, set the channel separately:

```cpp
IRCMessage msg(MessageType::Part, reason, nick);
msg.setChannel(chanName);
```

**Fix in `IRCMessage.cpp`** — Part formatting changes to "left the channel" with the reason in parentheses (matching the Quit format):

```cpp
case MessageType::Part:
    result = (!m_text.isEmpty()) ? QString("%1 left %2 (%3)").arg(m_sender).arg(m_channel).arg(m_text)
                                 : QString("%1 left %2").arg(m_sender).arg(m_channel);
    break;
```

(`m_channel` is already a member.)

---

### [ ] M-4. Display the topic setter from RPL 333

**File:** `backend/NetworkManager.cpp`, function `handleNumericReply` for numeric 333, lines ~829–832.

**The bug.** Current code:

```cpp
} else if (num == 333) {
    QString channel = params.value(1, "");
    QString timestamp = params.value(2, "");
    QString setter = params.value(3, "");
}
```

The variables are read into locals and the function falls through to the closing brace with nothing emitted. The user never sees who set the topic.

**Fix.** Emit it as a server-channel message (or a system message in the channel tab). Simplest:

```cpp
} else if (num == 333) {
    QString channel = params.value(1, "");
    QString setter = params.value(2, "");
    QString timestamp = params.value(3, "");
    QDateTime when = QDateTime::fromSecsSinceEpoch(timestamp.toLongLong());
    QString msgText = QString("Topic set by %1 on %2")
        .arg(setter.section('!', 0, 0))
        .arg(when.toString(Qt::ISODate));
    IRCMessage msg(MessageType::Topic, msgText, "");
    msg.setChannel(channel);
    emit channelMessage(channel, msg);
}
```

Note the param order on most servers is `<client> <channel> <setter> <timestamp>`, so params[1]=channel, params[2]=setter, params[3]=timestamp. (The current code had params[2] and params[3] mislabelled.)

---

### [ ] M-5. Handle RPL 001 (RPL_WELCOME) and update `m_nick` from it

**File:** `backend/NetworkManager.cpp`, function `handleNumericReply`, near the top of the if-chain (currently the chain starts at `if (num == 2)`).

**The bug.** Numeric 001 is the welcome line. Its first parameter is the nick the server has assigned us — possibly different from what we asked for (truncated, or the server may have appended a suffix to avoid a collision). The current code skips 001 entirely; `m_nick` continues to hold whatever we sent in the `NICK` command. Local-echo suppression, CTCP target matching and query routing all use `m_nick` and will all be wrong if the server assigned a different nick.

**Fix.** Add a branch *before* the `num == 2` branch:

```cpp
if (num == 1) {
    QString assignedNick = params.value(0, "");
    if (!assignedNick.isEmpty() && assignedNick != m_nick) {
        m_nick = assignedNick;
        emit nickSet(m_nick);
    }
    QString welcome = params.value(1, "");
    if (welcome.startsWith(':')) welcome = welcome.mid(1);
    emit serverChannelMessage("RPL 001 (Welcome): " + welcome);
} else if (num == 2) {
```

(Convert the existing `if` to `else if`.)

---

### [ ] M-6. Make `IRCChannel::findUser` safer

**File:** `backend/IRCChannel.h` and `backend/IRCChannel.cpp`, function `findUser`.

**The bug.** Current declaration:

```cpp
IRCUser* findUser(const QString& nick);
```

The implementation returns `&m_users[i]`. That's a pointer into a `QList`. Any subsequent `addUser`/`removeUser`/`clear` call invalidates it. Today every caller uses the pointer immediately and there's no interleaving, but the contract is unsafe and a future caller could crash.

**Fix.** Return the index instead, and add a separate `userAt(int)` accessor:

```cpp
// IRCChannel.h
int findUserIndex(const QString& nick) const;
IRCUser* userAt(int index);                    // pointer valid until next mutation
const IRCUser* userAt(int index) const;

// IRCChannel.cpp
int IRCChannel::findUserIndex(const QString& nick) const {
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].nick() == nick) return i;
    }
    return -1;
}

IRCUser* IRCChannel::userAt(int index) {
    if (index < 0 || index >= m_users.size()) return nullptr;
    return &m_users[index];
}
```

Then update callers (currently only `applyMode` and `MainWindow::onUserChangedNick` after H-4):

```cpp
int idx = ch->findUserIndex(nick);
if (idx >= 0) {
    IRCUser* user = ch->userAt(idx);
    user->setUserPrefix(...);
}
```

If the refactor is too much, at minimum keep the existing API but document the lifetime contract in the header. (The refactor is preferred.)

---

### [ ] M-7. Make `ColorRole` actually return a colour (or remove it)

**File:** `backend/IRCMessageModel.cpp`, function `data`, around line 47.

**The bug.** Current code:

```cpp
case Qt::DisplayRole:
    return msg.coloredText();   // HTML string
case TypeRole:
    return static_cast<int>(msg.type());
case ColorRole:
    return msg.coloredText();   // same HTML string — useless
```

`ColorRole` returns the same HTML string as `DisplayRole`. Any delegate that asks for `ColorRole` expecting a `QColor` gets back a string and can't use it.

**Fix.** Map `MessageType` to a `QColor` (matching the colours the HTML uses):

```cpp
case ColorRole: {
    switch (msg.type()) {
    case MessageType::NickChange: return QColor("#FF8888");
    case MessageType::Join:       return QColor("#88FF88");
    case MessageType::Part:
    case MessageType::Quit:
    case MessageType::Kick:       return QColor("#FF8888");
    case MessageType::Mode:
    case MessageType::Topic:
    case MessageType::TopicSet:   return QColor("#8888FF");
    case MessageType::Error:      return QColor("#FF0000");
    case MessageType::Notice:     return QColor("#AAAAAA");
    case MessageType::System:     return QColor("#888888");
    default:                       return QColor();
    }
}
```

Add `#include <QColor>` to the .cpp if not already present. If no caller uses `ColorRole`, the alternative is to delete the role entirely from the enum and the `data()` switch. Either fix is acceptable; pick one.

---

## LOW

### [ ] L-1. Stop sending an explicit NAMES after every JOIN

**File:** `backend/NetworkManager.cpp`, function `joinChannel`, lines ~83–86.

**The bug.** Every RFC-compliant IRC server automatically sends `353` + `366` (RPL_NAMREPLY + RPL_ENDOFNAMES) after a successful JOIN. The explicit `NAMES` command is unnecessary and produces a duplicate name list.

**Fix.** Delete the second line:

```cpp
void NetworkManager::joinChannel(const QString& channel) {
    sendCommand("JOIN", QStringList() << channel);
    // (no explicit NAMES — server sends 353+366 automatically)
}
```

If a `/names` slash command is wanted from the user, that path can call `sendCommand("NAMES", ...)` separately.

---

### [ ] L-2. Remove the redundant `m_pingTimer->start()` inside `onPingTimeout`

**File:** `backend/NetworkManager.cpp`, function `onPingTimeout`, lines ~279–282.

**The bug.** Current code:

```cpp
void NetworkManager::onPingTimeout() {
    sendRaw("PING :" + m_host + "\r\n");
    m_pingTimer->start();   // unnecessary
}
```

`m_pingTimer` is a repeating `QTimer` (`setSingleShot(true)` is never called on it). It auto-fires at its interval. Calling `start()` again restarts the interval counter from zero, harmlessly but pointlessly.

**Fix.** Remove the `m_pingTimer->start()` line:

```cpp
void NetworkManager::onPingTimeout() {
    sendRaw("PING :" + m_host + "\r\n");
}
```

---

### [ ] L-3. Don't add a trailing space when there are no MODE params

**File:** `backend/NetworkManager.cpp`, function `handleMode`, line ~632.

**The bug.** Current code:

```cpp
emit channelMode(target, mode + " " + modeParams.join(' '));
```

When `modeParams` is empty, `join(' ')` returns `""`, so the string ends up as `"+n "` with a trailing space. That string gets shoved into the tab title via `ChannelTab::setMode`, leaving a visible trailing space.

**Fix.** Only append the space-joined params if non-empty:

```cpp
QString modeStr = modeParams.isEmpty() ? mode : (mode + " " + modeParams.join(' '));
emit channelMode(target, modeStr);
```

Apply the same fix to the second `emit channelMessage` block in the same function (line ~640) where the same pattern is used to build the IRCMessage text.

---

### [ ] L-4. Mute CAP-negotiation chatter in the server tab

**File:** `backend/NetworkManager.cpp`, lines ~296, 763, 767, ~290 in `onCapReqTimeout`.

**The bug.** Every connection logs to the server tab:

```
Capabilities requested: sasl, server-time, echo-message, ...
Capabilities acknowledged: ...
```

This is implementation detail. End users don't need to see it.

**Fix.** Replace the four `emit serverChannelMessage("Capabilities ...")` calls with `qDebug()` (or just delete them). For example:

```cpp
qDebug() << "Capabilities requested:" << acceptedCaps.join(", ");
```

`#include <QDebug>` if not already present.

---

### [ ] L-5. Remove the unused `m_trafficLogFile` member

**File:** `backend/NetworkManager.h`, line ~144.

**The bug.** `QString m_trafficLogFile;` is declared, never assigned, never read. Dead code.

**Fix.** Delete the line.

---

### [ ] L-6. Remove the wrong `[[maybe_unused]]` on `prefix`

**File:** `backend/NetworkManager.cpp`, function `handleNumericReply` declaration, line ~773.

**The bug.** Current:

```cpp
void NetworkManager::handleNumericReply(const QString& numeric,
        [[maybe_unused]] const QString& prefix,
        const QStringList& params, const QString& serverTime) {
```

`prefix` *is* used (line ~822: `prefix.section('!', 0, 0)` for numeric 332). The attribute silences a warning that wouldn't fire anyway, and misleads readers into thinking it's unused.

**Fix.** Drop the attribute:

```cpp
void NetworkManager::handleNumericReply(const QString& numeric,
        const QString& prefix,
        const QStringList& params, const QString& serverTime) {
```

Apply the same change in the header (`NetworkManager.h` ~line 111) for consistency.

---

### [ ] L-7. Don't leak test scaffolding into the production header

**File:** `backend/NetworkManager.h`, lines ~113 and ~149.

**The bug.** The header has:

```cpp
virtual void sendRaw(const QString& data);
...
friend class TestIrcParser;
```

Both exist purely so the test subclass can override `sendRaw` and reach private state. Production code pays for a vtable slot it doesn't need, and `friend` opens the entire private interface.

**Fix.** Two acceptable approaches; pick one:

**Option A (preferred).** Make the test subclass live in the same translation unit as the test, and avoid `friend` by exposing a *protected* test-only seam:

```cpp
// in NetworkManager.h:
protected:
    virtual void sendRaw(const QString& data);   // protected, not private
private:
    // remove the friend class declaration
```

The test can then subclass `NetworkManager` and access `sendRaw` through the `protected` access. Anything else the test needs should also be moved to `protected`, or accessed via a public testing helper.

**Option B.** Leave it as-is (test scaffolding *is* the only override) but add a comment above each one explaining why:

```cpp
// virtual to allow tests to capture sent commands; do not subclass elsewhere.
virtual void sendRaw(const QString& data);
...
// gives test_irc_parser.cpp access to private parse helpers.
friend class TestIrcParser;
```

Option A is the cleaner long-term answer. Implement A.

---

### [ ] L-8. Strip leading `:` from the trailing param in the no-prefix branch of `parseMessage`

**File:** `backend/NetworkManager.cpp`, function `parseMessage`, lines ~373–387.

**The bug.** When a line starts with `:` (server prefix), the trailing-param handler at line ~356 strips the `:`:

```cpp
if (rest.startsWith(':')) {
    QString trailing = rest.mid(1);   // strip ':'
    params.append(trailing);
    break;
}
```

The else-branch (no prefix) builds the trailing param differently and *keeps* the `:`:

```cpp
QString trailing = params.takeAt(i);   // ":sasl"
while (i < params.size()) {
    trailing += ' ' + params.takeAt(i);
    ++i;
}
params.append(trailing);   // ":sasl server-time"
```

So `params` from a prefixless line have a leading `:` on the trailing param; from a prefixed line they don't. Every handler that processes a prefixless line must defensively strip `:`. The CAP LS handler does, but it's silent breakage waiting to happen for any new handler.

**Fix.** Strip the `:` in the else-branch trailing construction:

```cpp
QString trailing = params.takeAt(i);
if (trailing.startsWith(':')) trailing = trailing.mid(1);
while (i < params.size()) {
    trailing += ' ' + params.takeAt(i);
    ++i;
}
params.append(trailing);
```

After this fix, the corresponding ":"-strip logic in `handleCapCommand` (lines ~729–731) becomes dead code and can be removed.

---

### [ ] L-9. Make null-before-delete consistent in traffic-log shutdown

**File:** `backend/NetworkManager.cpp`, functions `setTrafficLogDir` and `clearTrafficLog`.

**The bug.** `setTrafficLogDir` and `clearTrafficLog` both do shutdown of the existing file/stream, but use slightly different sequences. The order should always be: delete the dependent first (`m_trafficLogStream` writes to `m_trafficLog`), then the file, then null both.

**Fix.** Extract a helper and call it from both places:

```cpp
void NetworkManager::closeTrafficLog() {
    if (m_trafficLogStream) {
        m_trafficLogStream->flush();
        delete m_trafficLogStream;
        m_trafficLogStream = nullptr;
    }
    if (m_trafficLog) {
        m_trafficLog->close();
        delete m_trafficLog;
        m_trafficLog = nullptr;
    }
}
```

Add `void closeTrafficLog();` to the private section of `NetworkManager.h`. Replace the inline cleanup in `setTrafficLogDir`, `clearTrafficLog`, and the destructor with a single call to `closeTrafficLog()`.

---

### [ ] L-10. De-duplicate `MAX_MESSAGES`

**File:** `backend/IRCChannel.h` line ~30, and `backend/IRCMessageModel.h` line ~34.

**The bug.** Both classes define the same constant independently:

```cpp
// IRCChannel.h
static const int MAX_MESSAGES = 10000;

// IRCMessageModel.h
static const int MAX_MESSAGES = 10000;
```

If one is changed, the other silently diverges, leading to subtly different trim points in the channel's persistent log vs. the model's display.

**Fix.** Pick one home and have the other refer to it. Easiest: keep `IRCMessageModel::MAX_MESSAGES` and have `IRCChannel.cpp` reference it, or define the constant in a shared header. Simplest:

```cpp
// IRCChannel.h — delete the local MAX_MESSAGES, add:
#include "IRCMessageModel.h"
...
// in IRCChannel.cpp: use IRCMessageModel::MAX_MESSAGES
if (m_messages.size() > IRCMessageModel::MAX_MESSAGES) { ... }
```

If that creates a circular include (it shouldn't — IRCMessageModel.h already includes IRCChannel.h), put the constant in a tiny new header `backend/IRCConstants.h` and include it in both places.

---

### [ ] L-11. Normalise indentation across the codebase

**Files:** `backend/NetworkManager.cpp` (esp. ~284–304, ~468), `backend/IRCMessage.cpp` (~43–44, ~92–96), `frontend/MainWindow.cpp` (~86, 99, 103, 165, 180, 367), `frontend/ChannelTab.cpp` (~6, 27), `frontend/ChatWidget.cpp` (~56, 60, 68).

**The bug.** Mixed 2-space, 4-space, and 1-space-off-by-one indentation, accumulated from edits made in multiple editors. No functional impact but makes diffs noisy and code harder to read.

**Fix.** Run a code formatter over the codebase. Use clang-format with a 4-space, K&R-ish style. Add `.clang-format` to the repo root:

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 120
PointerAlignment: Left
AccessModifierOffset: -4
NamespaceIndentation: None
BreakBeforeBraces: Attach
AllowShortFunctionsOnASingleLine: InlineOnly
```

Then run:

```
clang-format -i backend/*.cpp backend/*.h frontend/*.cpp frontend/*.h src/*.cpp tests/*.cpp
```

Verify the build still passes (`cmake --build build`) and the tests still pass (`ctest`). Commit the format change as a single commit, separate from any logic changes.

---

## Verification

After applying all fixes:

1. `cmake --build build --parallel` must build cleanly with no warnings.
2. `ctest --output-on-failure` must pass all three test binaries.
3. Run the application against a real IRC server (`irc.libera.chat:6667`):
   - `/join #test` should populate the user list (no duplicate user list from L-1).
   - Outgoing messages should appear once when echo-message is negotiated (C-1).
   - The server tab should be quiet — no CAP chatter (L-4).
   - Topic and topic-setter line should both display (M-4).
   - Tab title should not have a trailing space when modes are simple (L-3).
   - A nick change of an op user should preserve the `@` in the sidebar (H-4).
