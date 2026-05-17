# QwenIRC — Audit Checklist (rounds 3–7, 2026-05-17)

Status key: `[ ]` open

Completed items are removed. Each item includes a user story, INCOSE/EARS requirements,
a concise bug description, a fix hint, and an acceptance test.

---

## HIGH

### [H-11] `tabCloseRequested` sends `/PART` with no channel argument — PART is silently dropped

**User story.**
As an IRC user, I want closing a channel tab to actually send `PART #channel` to the server, so that the server removes me from the channel and other users see me leave.

**Requirements (EARS).**
- **REQ-H11-1 (event-driven).** When the system sends PART on behalf of a tab-close, the system shall include the channel name as the PART parameter.
- **REQ-H11-2 (unwanted behaviour).** The system shall not send a bare `PART\r\n` with no channel, as the server will respond with ERR 461 (not enough parameters) and the client remains joined.

**Location.** `frontend/MainWindow.cpp:102` (`tabCloseRequested` lambda)

**Bug.**
```cpp
m_network->sendUserInput(tabName, "/PART");
```
`sendUserInput` receives `text = "/PART"`. It splits on spaces: `parts = ["PART"]`. The handler checks `cmd == "PART" && parts.size() >= 2` — size is 1, not ≥ 2, so the branch is not taken. Execution falls through to the generic `sendCommand(cmd, parts.mid(1))` = `sendCommand("PART", [])`, sending `PART\r\n` with no channel. The server ignores it (or responds 461). The client remains joined on the server while the UI tab is closed, leaving the user in a phantom state.

**Fix hint.** Call the API directly to avoid the string-parsing round-trip:
```cpp
m_network->sendCommand("PART", QStringList() << tabName);
```
Or pass the channel name in the text argument:
```cpp
m_network->sendUserInput(tabName, "/PART " + tabName);
```

**Acceptance.** Clicking the close button on `#general` causes the server to echo `PART #general`; other users see the local nick leave.

- [x] Done

---

### [H-12] `removeChannelTab` calls `QTabWidget::removeTab` but never deletes the `ChannelTab` widget — memory leak on every tab close

**User story.**
As a developer, I want every closed tab to release its memory, so that a long-running session does not accumulate unbounded widget objects.

**Requirements (EARS).**
- **REQ-H12-1 (event-driven).** When a tab is removed from the tab widget, the system shall delete the corresponding `ChannelTab` widget object.
- **REQ-H12-2 (unwanted behaviour).** The system shall not call only `QTabWidget::removeTab`, as that function removes the widget from the tab bar without deleting it (Qt documentation: "The page widget itself is not deleted").

**Location.** `frontend/MainWindow.cpp:490-513` (`removeChannelTab`)

**Bug.**
```cpp
m_channelTabs->removeTab(i);
break;
```
`QTabWidget::removeTab(i)` unparents the widget from the tab widget's internal stack but does not delete it. The `ChannelTab` instance (and all its child widgets: `ChatWidget`, `QLineEdit`, etc.) lives on as a parentless, invisible orphan.

**Fix hint.** Retrieve the widget pointer before removing, then schedule deletion:
```cpp
auto* tab = qobject_cast<ChannelTab*>(m_channelTabs->widget(i));
if (tab && tab->channelName().toLower() == norm) {
    m_channelTabs->removeTab(i);
    tab->deleteLater();
    break;
}
```

**Acceptance.** Closing 100 channel tabs in sequence does not grow the process's resident memory. Valgrind/AddressSanitizer reports no reachable `ChannelTab` objects after all tabs are closed.

- [x] Done

---

### [H-13]

**User story.**
As an IRC user, I want WHOIS to show the correct ident (username) for a queried nick, so that I can verify who I am talking to.

**Requirements (EARS).**
- **REQ-H13-1 (event-driven).** When the system receives RPL 311, the system shall emit `whoisIdent(nick, ident)` where `ident = params[2]` (the IRC username field), not `params[3]` (the hostname).

**Location.** `backend/NetworkManager.cpp` (RPL 311 handler, `num == 311` branch)

**Bug.**
RPL 311 is formatted as `:server 311 <me> <nick> <user> <host> * :<realname>`. After parsing:
- params[0] = me (requestor)
- params[1] = nick (target) ✓
- params[2] = user (ident/username) ✓
- params[3] = host (hostname) ← mislabelled as `realName`

```cpp
QString identNick = params.value(1, "");  // nick — correct
QString ident     = params.value(2, "");  // ident — read but never used
QString realName  = params.value(3, "");  // host, mislabelled as realName
emit whoisIdent(identNick, realName);     // emits (nick, hostname) — wrong
```
The variable `ident` (the actual username, e.g. `~alice`) is read but discarded. The signal receives the hostname instead.

**Fix hint.**
```cpp
QString nick     = params.value(1, "");
QString ident    = params.value(2, "");
emit whoisIdent(nick, ident);
```

**Acceptance.** `/whois alice` shows the correct ident `~alice` in the ident field, not the hostname.

- [x] Done

---

## MEDIUM

### [M-16] `tabCloseRequested` hardcodes `startsWith('#') || startsWith('&')` before the CHANTYPES loop

**User story.**
As an IRC user on a network with unusual channel prefixes, I want closing a channel tab to send PART regardless of the prefix character, so that I am properly parted on the server.

**Requirements (EARS).**
- **REQ-M16-1 (event-driven).** When determining whether a tab represents a channel, the system shall check the tab name against all characters in the ISUPPORT `CHANTYPES` string without any additional pre-filter.
- **REQ-M16-2 (unwanted behaviour).** The system shall not reject valid channel tabs based on a hardcoded set of prefix characters (`#`, `&`) when CHANTYPES may contain other characters.

**Location.** `frontend/MainWindow.cpp:92` (`tabCloseRequested` lambda)

**Bug.**
```cpp
if (tabName.startsWith('#') || tabName.startsWith('&')) {
    QString chanTypes = m_network->isupport().value("CHANTYPES", "#");
    bool isChannel = false;
    for (QChar ct : chanTypes) {
        if (tabName.startsWith(ct)) {
            isChannel = true;
            break;
        }
    }
    if (isChannel) {
        m_network->sendUserInput(tabName, "/PART");
    }
}
```
The outer `if` pre-filters to only `#` and `&`. If CHANTYPES is `#&!` and the user is in a `!` channel, the outer guard prevents PART from being sent even though the inner CHANTYPES loop would have matched.

**Fix hint.** Remove the outer `if` and combine H-11's fix:
```cpp
QString chanTypes = m_network->isupport().value("CHANTYPES", "#");
bool isChannel = std::any_of(chanTypes.cbegin(), chanTypes.cend(),
    [&](QChar ct) { return tabName.startsWith(ct); });
if (isChannel) {
    m_network->sendCommand("PART", QStringList() << tabName);
}
```

**Acceptance.** On a server with `CHANTYPES=#&!`, closing a `!channel` tab sends `PART !channel\r\n`.

- [x] Done

---

### [M-17]

**User story.**
As an IRC user in a `&local` channel, I want notices addressed to that channel to appear in the channel tab, not open a spurious DM tab to the sender.

**Requirements (EARS).**
- **REQ-M17-1 (event-driven).** When routing an incoming NOTICE, the system shall determine whether the target is a channel by checking against the full ISUPPORT `CHANTYPES` string, not only its first character.

**Location.** `backend/NetworkManager.cpp:578` (`handleNotice`)

**Bug.**
```cpp
QChar chantype = m_isupport["CHANTYPES"].isEmpty() ? QChar('#') : m_isupport["CHANTYPES"].front();
// ...
if (target.startsWith(chantype) && channel(target)) {
```
`m_isupport["CHANTYPES"].front()` returns only the first character (`#`). A notice to `&local` has `target[0] = '&'`, which does not equal `#`, so the notice is routed to a DM/server-message branch.

**Fix hint.**
```cpp
QString chanTypes = m_isupport.value("CHANTYPES", "#");
bool isChannelTarget = !chanTypes.isEmpty() && chanTypes.contains(target[0]) && channel(target);
if (isChannelTarget) {
    emit noticeReceived(sender, text);
    emit channelMessage(target, msg);
}
```

**Acceptance.** A notice to `&local` appears in the `&local` channel tab. A private notice from `alice` opens a DM tab.

- [x] Done

---

### [M-18]

**User story.**
As an IRC user, I want mIRC color codes and all standard IRC formatting to be fully stripped from displayed text, so that no raw numbers or control characters appear in the message view.

**Requirements (EARS).**
- **REQ-M18-1 (ubiquitous).** When `\x03` (color) appears in text, the system shall skip the optional one-or-two-digit foreground color number and, if present, a comma followed by an optional one-or-two-digit background color number, in addition to the `\x03` byte itself.
- **REQ-M18-2 (ubiquitous).** The system shall strip underline (`\x1F`, code 31) and reverse-video (`\x16`, code 22) in addition to the currently stripped codes.

**Location.** `backend/IRCMessage.cpp:133` (`stripIrcFormatting`)

**Bug.**
```cpp
if (u == 2 || u == 3 || u == 15 || u == 17 || u == 27 || u == 29 || u == 30) {
    continue;
}
```
The `\x03` byte is removed, but the digit arguments that follow (e.g. `\x034,1hello` → digits `4,1`) are not consumed, producing `4,1hello` instead of `hello`. The stripped set also omits:
- 22 (`\x16`) reverse video
- 31 (`\x1F`) underline

**Fix hint.**
```cpp
if (u == 3) {  // color — skip optional fg[,bg] digits
    ++i;
    int digits = 0;
    while (i < text.size() && text[i].isDigit() && digits < 2) { ++i; ++digits; }
    if (i < text.size() && text[i] == ',') {
        ++i; digits = 0;
        while (i < text.size() && text[i].isDigit() && digits < 2) { ++i; ++digits; }
    }
    --i;
} else if (u == 2 || u == 15 || u == 16 || u == 17 || u == 22 || u == 29 || u == 30 || u == 31) {
    // bold, reset, (unused), monospace, reverse, italic, strikethrough, underline
}
```

**Acceptance.** `\x034red text\x03` renders as `red text`. `\x1Funderlined\x1F` renders as `underlined`.

- [x] Done

---

### [M-19]

**User story.**
As an IRC user, I want all message types — including join/kick/nick-change lines — to display cleanly without raw control characters, even when the underlying text contains IRC formatting codes.

**Requirements (EARS).**
- **REQ-M19-1 (ubiquitous).** The system shall apply `stripIrcFormatting` to the text of every `IRCMessage` type before rendering, not only to `Part`, `Quit`, `Mode`, `Topic`, and `Notice`.

**Location.** `backend/IRCMessage.cpp:66-119` (`coloredText`)

**Bug.**
`coloredText` calls `stripIrcFormatting(formattedText())` for `Part`, `Quit`, `Mode`, `Topic`, and `Notice`. The remaining types call only `formattedText().toHtmlEscaped()` without stripping:

| Type | Stripped? | Risk |
|------|-----------|------|
| `NickChange` | ✗ | nick or new-nick with formatting |
| `Join` | ✗ | low (channel name only) |
| `Kick` | ✗ | kick reason from kicker — user-supplied |
| `TopicSet` | ✗ | low (auto-generated) |
| `Error` | ✗ | low (auto-generated) |
| `System` | ✗ | low (auto-generated) |

A kick reason like `\002you violated rule 4\002` renders with raw `\x02` characters in the Kick line.

**Fix hint.** Apply stripping in every remaining branch:
```cpp
// Replace in NickChange, Join, Kick, TopicSet, Error, System:
formattedText().toHtmlEscaped()
// with:
IRCMessage::stripIrcFormatting(formattedText()).toHtmlEscaped()
```

**Acceptance.** A kick reason containing bold or colour codes renders as plain text. A nick change line containing a nick with underline codes renders cleanly.

- [x] Done

---

### [M-20]

**User story.**
As an IRC user in a channel where the server advertises `multi-prefix`, I want voiced-ops (`@+alice`) who leave or change nick to be correctly removed or renamed in my user list, so the list stays accurate.

**Requirements (EARS).**
- **REQ-M20-1 (event-driven).** When removing or renaming a user in the sidebar user list, the system shall strip all leading prefix characters (not only the first one) before comparing the nick.
- **REQ-M20-2 (unwanted behaviour).** The system shall not stop stripping after the first prefix character when the entry may carry two or more prefix characters (e.g. `@+alice`).

**Location.** `frontend/MainWindow.cpp:424-426` (`onUserLeft`), `frontend/MainWindow.cpp:456-458` (`onUserChangedNick`)

**Bug.**
```cpp
if (!text.isEmpty() && !symbols.isEmpty() && symbols.contains(text[0])) {
    text = text.mid(1);
}
if (text == nick) { ... }
```
For a list entry `@+alice`, `text[0]` is `@` — a symbol — so `text` becomes `+alice`. The comparison `+alice == alice` fails, so the user is not removed. The previous implementation used `QRegularExpression re("[~&@%+]"); text.replace(re, "")` which stripped all prefix characters in one pass.

**Fix hint.** Replace the `if` with a `while`:
```cpp
while (!text.isEmpty() && !symbols.isEmpty() && symbols.contains(text[0])) {
    text = text.mid(1);
}
```
Apply the same change in both `onUserLeft` (line 424) and `onUserChangedNick` (line 456).

**Acceptance.** When `@+alice` parts a channel, the entry `@+alice` is removed from the user list. When `@+alice` changes nick to `bob`, the entry is updated to `bob` (or the new prefix + `bob`).

- [x] Done

---

## LOW

### [L-13]

**User story.**
As a maintainer, I want unreachable private member functions to be removed so that the codebase does not carry misleading dead code.

**Requirements (EARS).**
- **REQ-L13-1 (ubiquitous).** The system shall not retain private member functions that have no callers.

**Location.** `backend/NetworkManager.cpp:1108`, `backend/NetworkManager.h:147` (private declaration)

**Bug.**
`extractModePrefix` was used in the RPL 353 loop to extract a single leading prefix character. That loop was rewritten to strip all leading prefix characters inline using a `while` loop. `extractModePrefix` has no remaining callers.

**Fix hint.** Remove the definition from `NetworkManager.cpp` and the declaration from the private section of `NetworkManager.h`.

**Acceptance.** A clean build with `-Wunused-function` emits no warning for `extractModePrefix`.

- [x] Done

---

### [L-14]

**User story.**
As an IRC user on a server with `CHANTYPES=#&`, I want `/TOPIC` typed in a `&local` channel tab to be sent to the server, so I can view or change the channel topic.

**Requirements (EARS).**
- **REQ-L14-1 (event-driven).** When the system processes a `/TOPIC` command from user input, the system shall determine whether the current context is a channel by checking against the full ISUPPORT `CHANTYPES` string, not only its first character.

**Location.** `backend/NetworkManager.cpp:125` (`sendUserInput` TOPIC branch)

**Bug.**
```cpp
QChar chanType = m_isupport["CHANTYPES"].isEmpty() ? QChar('#') : m_isupport["CHANTYPES"].front();
if (!context.isEmpty() && context != "Server" && context[0] == chanType) {
```
`m_isupport["CHANTYPES"].front()` returns only `#`. When the user types `/TOPIC` in a `&local` tab, `context[0] = '&'` does not equal `#`, so the TOPIC command is silently not sent. The same `.front()` pattern is already fixed in `joinChannel` (line 88) and `handleNotice` is tracked under M-17.

**Fix hint.**
```cpp
QString chanTypes = m_isupport.value("CHANTYPES", "#");
if (!context.isEmpty() && context != "Server"
    && !chanTypes.isEmpty() && chanTypes.contains(context[0])) {
```

**Acceptance.** Typing `/TOPIC` in a `&local` channel tab sends `TOPIC &local\r\n` to the server.

- [x] Done

---

## Verification checklist

After all items above are resolved, run the following to confirm no regressions:

```
[x] cmake --build build --parallel  — builds clean, zero warnings on GCC (-Werror)
[x] ctest --output-on-failure       — all three test binaries pass
[ ] Manual: connect to irc.libera.chat:6667 (plain)
    [ ] Close a channel tab — server echoes PART with correct channel name (H-11)
    [ ] /whois alice shows ident (~alice), not hostname, in second field (H-13)
    [ ] Message with \x034red text\x03 displays as "red text" (M-18)
    [ ] Nick-change line with bold code renders without control characters (M-19)
    [ ] Kick with bold reason renders without control characters (M-19)
[ ] Manual: open and close 20 tabs
    [ ] Process memory does not grow monotonically after closing tabs (H-12)
[ ] Manual: channel with ops+voiced users (multi-prefix)
    [ ] @+alice parting the channel removes the entry from the user list (M-20)
    [ ] @+alice changing nick updates the user list entry correctly (M-20)
[ ] Manual (if server supports CHANTYPES=!): close a !channel tab
    [ ] Server echoes PART for !channel (M-16)
```
