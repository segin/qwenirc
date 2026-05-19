# QwenIRC — Audit Checklist (rounds 3–10, 2026-05-18)

Status key: `[ ]` open

Completed items are removed. Each item includes a user story, INCOSE/EARS requirements,
a concise bug description or feature description, a fix/implementation hint, and an acceptance test.

---

## MEDIUM

### [M-21] User list is never sorted — joins appear in server-arrival order, not by privilege then alphabetically

**User story.**
As an IRC user, I want the channel user list to show operators first, then voiced users, then regular users, each group sorted alphabetically, so I can immediately see who has elevated privileges.

**Requirements (EARS).**
- **REQ-M21-1 (event-driven).** When the system populates or updates the user list for any reason (NAMES list, tab switch, user join, nick change), it shall sort the entries by descending privilege order as defined by the ISUPPORT `PREFIX` spec, then case-insensitively alphabetically within each privilege tier.
- **REQ-M21-2 (ubiquitous).** The system shall derive the privilege order from the ordered symbol string in the `PREFIX` ISUPPORT token (e.g. `(qaohv)~&@%+` → `~` outranks `&` outranks `@` etc.) rather than from a hardcoded symbol set.

**Location.** `frontend/MainWindow.cpp` — four unsorted population sites:
- Line 204 (`userReceived` lambda) — plain nick appended, never sorted
- Line 221-226 (tab-change `currentChanged` lambda) — reads `ch->users()` insertion order
- Line 405-406 (`onUserJoined`) — `addItem` at the end of the list
- Line 521-530 (`onNamesReceived`) — adds in the order the server sent NAMES

**Bug.**
None of the four code paths that add users to `m_userList` call any sorting function. A channel with operators, voiced users, and regular users displays them in whatever order the server happened to list them in RPL 353, with new joiners always tacked at the bottom.

Example: if the server sends `NAMES #foo` as `alice bob @carol +dave`, the list shows exactly that order. `@carol` appears below unvoiced `alice` and `bob`. After `eve` joins, she is appended after `+dave`.

**Fix hint.**

Add a private helper `MainWindow::sortUserList()` and call it at each population site:

```cpp
void MainWindow::sortUserList() {
    // Extract ordered symbol string from PREFIX, e.g. "~&@%+" from "(qaohv)~&@%+"
    QString prefixSpec = m_network->isupport().value("PREFIX", "(qaohv)~&@%+");
    int parenClose = prefixSpec.indexOf(')');
    QString symbols = (parenClose >= 0) ? prefixSpec.mid(parenClose + 1) : QString("~&@%+");

    auto sortKey = [&](const QString& entry) -> QPair<int, QString> {
        // rank: position in symbols string (lower = higher privilege); no prefix = last
        int rank = symbols.size();
        if (!entry.isEmpty() && symbols.contains(entry[0]))
            rank = symbols.indexOf(entry[0]);
        // bare nick for alpha comparison
        QString nick = entry;
        while (!nick.isEmpty() && symbols.contains(nick[0]))
            nick = nick.mid(1);
        return {rank, nick.toLower()};
    };

    QStringList items;
    items.reserve(m_userList->count());
    for (int i = 0; i < m_userList->count(); ++i)
        items << m_userList->item(i)->text();
    std::stable_sort(items.begin(), items.end(), [&](const QString& a, const QString& b) {
        return sortKey(a) < sortKey(b);
    });
    m_userList->clear();
    m_userList->addItems(items);
}
```

Then add `sortUserList()` calls:
- After the `for` loop in the `currentChanged` lambda (tab switch, ~line 226)
- After the `for` loop in `onNamesReceived` (~line 530)
- After `m_userList->addItem(entry)` in `onUserJoined` (~line 406)
- After the `setText` call in `onUserChangedNick` (~line 454)

Declare the helper in `frontend/MainWindow.h` as a private member function.

**Acceptance.**
- Channel with `@carol`, `+dave`, `alice`, `bob` displays as: `@carol`, `+dave`, `alice`, `bob`.
- After unvoiced `eve` joins, list becomes: `@carol`, `+dave`, `alice`, `bob`, `eve`.
- On a server with `PREFIX=(qaohv)~&@%+`, a channel owner `~quinn` sorts above `@carol`.

- [ ] Done

---

### [M-22] Mode changes that alter a user's prefix are not reflected in the user list

**User story.**
As an IRC user, I want the user list to update immediately when someone is opped or deopped, so I can always see the current privilege state without switching tabs.

**Requirements (EARS).**
- **REQ-M22-1 (event-driven).** When the system processes a MODE message that changes a channel user's prefix (e.g. `+o`, `-v`, `+h`), the system shall update that user's entry in the sidebar user list for any tab currently displaying that channel.
- **REQ-M22-2 (unwanted behaviour).** The system shall not require the user to switch away and back to a channel tab to see a prefix change take effect.

**Location.** `backend/NetworkManager.cpp` (`handleMode`), `frontend/MainWindow.cpp` (no handler for prefix-change events)

**Bug.**
`NetworkManager::handleMode` calls `ch->applyMode(...)` which calls `IRCUser::setUserPrefix(...)` on the relevant user object, then emits `channelMessage(target, msg)` (the mode text line). No signal is emitted that carries user-prefix-change information to the frontend. The `MainWindow` has no slot connected to any such signal. The user list is therefore stale after any `+o`, `-o`, `+v`, `-v`, `+h`, `-h` etc. operation. The correct prefix only appears after a tab switch (which re-reads from `ch->users()`) or a reconnect.

**Fix hint.**

Emit a dedicated signal from `handleMode` after applying user-prefix changes, and connect it in `MainWindow`.

In `NetworkManager.h` signals:
```cpp
void userPrefixChanged(const QString& channel, const QString& nick, const QString& newPrefix);
```

In `NetworkManager.cpp::handleMode`, after `ch->applyMode(...)`:
```cpp
// Re-scan applied prefix modes to emit per-user notifications
for (int mi = 0; mi < modeStr.size(); ++mi) {
    QChar c = modeStr[mi];
    // (reuse the same prefix-mode detection logic as applyMode)
    if (modeMap.contains(c) && paramIdx < modeParams.size()) {
        IRCUser* u = ch->findUser(modeParams[paramIdx]);
        if (u)
            emit userPrefixChanged(target, u->nick(), u->userPrefix());
        ++paramIdx;
    }
}
```

Alternatively — and more simply — emit a single signal that the channel's user list changed, and have `MainWindow` rebuild it from `ch->users()`:

```cpp
// NetworkManager.h:
void channelUsersChanged(const QString& channel);

// handleMode, after applyMode:
emit channelUsersChanged(target);
```

In `MainWindow::initializeUI()`:
```cpp
connect(m_network, &NetworkManager::channelUsersChanged, this, [this](const QString& channel) {
    if (m_currentChannel.toLower() != channel.toLower()) return;
    m_userList->clear();
    auto* ch = m_network->channel(channel);
    if (!ch) return;
    for (const IRCUser& u : ch->users()) {
        QString prefix = u.userPrefix();
        m_userList->addItem(prefix.isEmpty() ? u.nick() : prefix + u.nick());
    }
    sortUserList();
});
```

**Acceptance.**
- `/mode #channel +o alice` immediately changes `alice` to `@alice` in the user list without requiring a tab switch.
- `/mode #channel -v dave` immediately removes the `+` from `+dave`.

- [ ] Done

---

## Verification checklist

After all items above are resolved, run the following to confirm no regressions:

```
[ ] cmake --build build --parallel  — builds clean, zero warnings on GCC (-Werror)
[ ] ctest --output-on-failure       — all test binaries pass
[ ] Manual: connect to irc.libera.chat:6667 (plain)
    [ ] Join a channel with ops, voiced, and regular users
    [ ] User list shows @ops first, then +voiced, then regular, each group A–Z (M-21)
    [ ] After joining, new users appear in correct sorted position (M-21)
    [ ] After /mode #channel +o alice, alice immediately shows as @alice (M-22)
    [ ] After /mode #channel -v dave, +dave immediately shows as dave (M-22)
```
