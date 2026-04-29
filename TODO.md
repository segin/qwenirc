# QwenIRC — Requirements & Task Checklist

Requirements use EARS notation:
- **Ubiquitous:** The [system] shall [action]
- **Event-driven:** When [trigger], the [system] shall [action]
- **State-driven:** While [state], the [system] shall [action]
- **Unwanted:** If [condition], the [system] shall [action]

Priority: **P1** critical → **P4** low  
Status: `[ ]` open · `[x]` done

---

## 1. CRITICAL — Client Cannot Connect

### REQ-CAP-01 · P1 — CAP LS capability list must be split before intersection

**EARS:** When the server sends a `CAP LS` reply, the system shall split the trailing parameter on spaces before intersecting with the locally supported capability list.

**User story:** As a user, I want IRCv3 capabilities to be negotiated on every connection so that server-time, echo-message, and other features work correctly.

**Root cause:** `params.mid(2)` yields a one-element list containing the entire space-separated cap string. `availableCaps.insert("sasl server-time …")` inserts one composite key. Intersection with individual cap names in `m_capSupported` always produces an empty set, so `CAP END` is sent immediately with nothing requested.  
**File:** `NetworkManager.cpp:684-709`

**Test requirements:**
- Given a simulated `CAP * LS :sasl server-time echo-message multi-prefix`, verify `m_activeCaps` contains individual entries after ACK.
- Verify `CAP REQ :server-time echo-message` (or subset) is sent, not `CAP REQ :` with empty list.
- Verify no request is sent for caps the server did not advertise.

- [x] In `handleCapCommand` LS branch, collect caps from `params.mid(2)`, then for each element call `.split(' ', Qt::SkipEmptyParts)` to get individual cap names before inserting into `availableCaps`.
- [x] Handle multiline CAP LS (`*` indicator at params[2]): accumulate caps across multiple LS responses; send REQ only when the non-`*` final LS arrives.

---

### REQ-CAP-02 · P1 — NICK/USER registration must follow CAP END, not server CAP END echo

**EARS:** When the client sends `CAP END`, the system shall immediately send `NICK` and `USER` registration commands.

**User story:** As a user, I want the client to complete IRC registration after capability negotiation so that I can actually join and chat.

**Root cause:** Registration logic sits in `else if (action == "END")` which handles a server-originated `CAP END`. Servers never send `CAP END`; only clients do. NICK/USER are therefore never sent. The client stalls permanently after the TLS/TCP handshake.  
**File:** `NetworkManager.cpp:710-733`

**Test requirements:**
- Given a simulated server that sends `CAP * LS :` (no caps), verify NICK and USER are sent after client sends `CAP END`.
- Given a server that sends `CAP * ACK :echo-message`, verify NICK and USER are sent after the ACK is processed.
- Verify NICK is sent before USER.
- Verify PASS is sent before NICK when a password is set.

- [x] Move PASS + NICK + USER + `m_pingTimer->start()` into a private `void sendRegistration()` method.
- [x] Call `sendRegistration()` at the end of the ACK handler (after storing caps and sending `CAP END`).
- [x] Call `sendRegistration()` in the no-caps branch of the LS handler (after sending `CAP END`).
- [x] Call `sendRegistration()` in the NAK handler (after sending `CAP END`).
- [x] Delete the dead `else if (action == "END")` branch.

---

### REQ-BUILD-01 · P1 — OpenSSL CMake variable must use correct capitalisation

**EARS:** When `find_package(OpenSSL)` succeeds, the system shall link `OpenSSL::SSL` and `OpenSSL::Crypto` into the qwenirc target.

**User story:** As a user, I want the TLS option to be compiled in when OpenSSL is present so that I can connect to servers requiring encrypted connections.

**Root cause:** `find_package(OpenSSL)` sets `OPENSSL_FOUND`; the CMake condition checks `OpenSSL_FOUND` (mixed case). The condition is always false; OpenSSL is never linked; `QSslSocket` operations silently fail at runtime.  
**File:** `CMakeLists.txt:19`

**Test requirements:**
- On a system with OpenSSL installed, verify CMake configure output does NOT print "OpenSSL not found".
- Verify `cmake --build build` links against `libssl` and `libcrypto` (`ldd build/qwenirc | grep ssl`).

- [x] Change `if(OpenSSL_FOUND)` to `if(OPENSSL_FOUND)`.

---

## 2. HIGH — Functional Bugs

### REQ-NUM-01 · P1 — Numeric reply params must be indexed from position 1, not 0

**EARS:** When a numeric server reply is received, the system shall parse channel names from `params[1]` and data from `params[2]` onwards, treating `params[0]` as the recipient nickname per RFC 2812 §5.

**User story:** As a user, I want channel topics, mode strings, ban lists, and user lists to display correctly so that I have accurate information about channel state.

**Root cause:** The Qwen pass applied partial fixes that still read `params[0]` as the channel name. `params[0]` is always the recipient nick. All affected handlers remain broken.  
**File:** `NetworkManager.cpp:741-865`

**Test requirements (per numeric):**
- 353: Given `:srv 353 me = #chan :@op +voice user`, verify channel = `#chan` and user list contains `op`, `voice`, `user` with correct prefixes.
- 366: Given `:srv 366 me #chan :End`, verify `namesComplete("#chan")` is emitted.
- 332: Given `:srv 332 me #chan :the topic`, verify `channelTopic("#chan", "the topic")` is emitted.
- 324: Given `:srv 324 me #chan +ns`, verify `channelMode("#chan", "+ns")` is emitted.
- 333: Given `:srv 333 me #chan setter 1234`, verify setter and timestamp are read correctly.
- 005: Given `:srv 005 me CHANTYPES=# CASEMAPPING=rfc1459 :supported`, verify both tokens are stored in `m_isupport`.

- [x] **353 RPL_NAMREPLY:** `channel = params.value(1)`, `users = params.value(2)`.
- [x] **366 RPL_ENDOFNAMES:** `channel = params.value(1)`.
- [x] **332 RPL_TOPIC:** `channel = params.value(1)`, `topic = params.value(2)`.
- [x] **324 RPL_CHANNELMODEIS:** `channel = params.value(1)`, `mode = params.value(2)`.
- [x] **333 RPL_TOPICWHOTIME:** `channel = params.value(1)`, `timestamp = params.value(2)`, `setter = params.value(3)`.
- [x] **329 RPL_CREATIONTIME:** `channel = params.value(1)`, `ts = params.value(2)`.
- [x] **331 RPL_NOTOPIC:** `channel = params.value(1)`.
- [x] **367/368 ban list:** channel index shifted to params.value(1).
- [x] **005 ISUPPORT:** start loop at `i = 1`, skip trailing `:` token.

---

### REQ-MODE-01 · P1 — `IRCChannel::applyMode` must parse mode strings correctly

**EARS:** When a MODE message is received for a channel, the system shall parse each mode character in sequence, tracking the add/remove state with `+`/`-` prefix characters, and shall apply user mode changes to the correct channel member using the corresponding parameter.

**User story:** As a user, I want `@` and `+` prefixes in the user list to update when ops or voice are set or removed so that I can identify privileged users.

**Root cause:** The current loop checks `if (isAdd || c == '-')` where `c` is the current character. This condition is only true when `c` is `+` or `-`. When `c` is a mode letter like `o`, the condition is false and the inner body never runs. No user mode is ever changed.  
**File:** `IRCChannel.cpp:56-81`

**Test requirements:**
- Given `applyMode("+o", ["nick1"], "setter")`, verify `findUser("nick1")->userPrefix() == "@"`.
- Given `applyMode("+v", ["nick2"], "setter")`, verify prefix is `"+"`.
- Given `applyMode("-o", ["nick1"], "setter")` after granting op, verify prefix is cleared.
- Given `applyMode("+ov", ["nick1", "nick2"], "setter")`, verify both users updated.
- Given `applyMode("+mb", ["key", "ban!mask"], "setter")`, verify no crash and no user prefix change.

- [ ] Rewrite the loop with a separate `bool adding = true` tracking variable:
  ```
  for each char c in modeStr:
      if c == '+': adding = true; continue
      if c == '-': adding = false; continue
      // c is a mode letter — consume param if applicable
  ```
- [ ] Map mode letter to prefix symbol using `m_isupport["PREFIX"]` (default `(ohv)@%+`); parse the `(letters)symbols` format to build the mapping.
- [ ] Set `user->setUserPrefix(adding ? symbol : "")` where `symbol` is looked up from the mapping.
- [ ] For non-user modes that take a param (`k`, `l`, `b`, `e`, `I`), consume but discard the param.

---

### REQ-TLS-01 · P1 — TLS selection must be forwarded from dialog to network layer

**EARS:** When the user enables the TLS checkbox and clicks Connect, the system shall pass `useTLS = true` to `NetworkManager::connectToServer`.

**User story:** As a user, I want my TLS checkbox selection to actually enable encrypted connections so that my password and messages are protected.

**Root cause:** `ServerDialog::connectRequested` emits 6 parameters including `bool useTLS`. `MainWindow::onConnect` is declared with only 5 parameters — `useTLS` is silently dropped by Qt's connection mechanism. `connectToServer` is always called with implicit `useTLS = false`.  
**Files:** `MainWindow.h:28-30`, `MainWindow.cpp:333-337`

**Test requirements:**
- Verify selecting the TLS checkbox and connecting results in `QSslSocket::connectToHostEncrypted` being called, not `QTcpSocket::connectToHost`.
- Verify port defaults to 6697 when TLS is checked (UX improvement, not blocking).

- [x] Add `bool useTLS` parameter to `MainWindow::onConnect` slot declaration in `MainWindow.h`.
- [x] Update `MainWindow::onConnect` body to pass `useTLS` to `m_network->connectToServer(...)`.
- [x] Set port default to 6697 when TLS checkbox is toggled (in `ServerDialog.cpp`).

---

### REQ-NICK-01 · P2 — Nick changes must update IRCChannel user lists

**EARS:** When a `NICK` message is received, the system shall update the user entry in every channel where that user is a member, replacing the old nick with the new nick.

**User story:** As a user, I want the user list to show current nicks after a rename so that I can correctly address channel members.

**Root cause:** `handleNick` scopes message delivery correctly (only to channels where the user is present) but never calls `ch->removeUser(oldNick)` or `ch->addUser(IRCUser(newNick))`. The channel's internal user list retains the old nick indefinitely.  
**File:** `NetworkManager.cpp:523-528`

**Test requirements:**
- After a NICK from `alice` to `alice_`, verify `IRCChannel::findUser("alice")` returns nullptr.
- Verify `IRCChannel::findUser("alice_")` returns a valid user.
- Verify `userChangedNick` signal is still emitted once.

- [x] Inside the `handleNick` channel loop, after the `findUser` check: copy the old user's ident/host, call `ch->removeUser(oldNick)`, construct `IRCUser(newNick, ident, host)` and call `ch->addUser(...)`.

---

### REQ-QUIT-01 · P2 — Quit reason must be included in the displayed message

**EARS:** When a `QUIT` message is received, the system shall display the quit reason in the channel message if one is provided.

**Root cause:** `handleQuit` constructs `IRCMessage(MessageType::Quit, "Quit", nick)` with the hardcoded text `"Quit"`. The parsed `reason` variable is discarded. `formattedText()` has a conditional for non-empty text that is never reached because text is always `"Quit"`.  
**File:** `NetworkManager.cpp:633`

**Test requirements:**
- Given `:alice QUIT :Gone to lunch`, verify displayed message contains "Gone to lunch".
- Given `:alice QUIT` (no reason), verify message is "alice quit" without a reason clause.

- [x] Change `IRCMessage msg(MessageType::Quit, "Quit", nick)` to `IRCMessage msg(MessageType::Quit, reason, nick)`.

---

### REQ-SSL-01 · P2 — SSL error handler must not crash on empty error list

**EARS:** If `onSslErrors` is called with an empty error list, the system shall emit a generic SSL error message rather than crashing.

**Root cause:** `errors.first()` on an empty `QList` is undefined behaviour.  
**File:** `NetworkManager.cpp:263-266`

**Test requirements:**
- Verify `onSslErrors({})` does not crash.
- Verify `onSslErrors({QSslError(QSslError::CertificateExpired)})` emits the error string.

- [x] Guard: `if (errors.isEmpty()) { emit serverError("SSL error"); return; }`
- [x] Change to `for (const auto& e : errors) { emit serverError("SSL: " + e.errorString()); }` then disconnect.

---

### REQ-CONN-01 · P2 — Pong timer must be stopped on disconnect

**EARS:** When the socket disconnects for any reason, the system shall stop the pong timer to prevent a spurious `connectionLost` signal.

**Root cause:** `onDisconnected` stops `m_pingTimer` but not `m_pongTimer`. A race between server close and pending pong timeout causes a second disconnect/lost signal.  
**File:** `NetworkManager.cpp:211-216`

**Test requirements:**
- Verify that closing the server connection while a PING is in-flight does not emit `connectionLost` after `disconnected`.

- [x] `m_pingTimer->stop()` is already present in `onDisconnected` (TODO used wrong variable name; actual timer is `m_pingTimer`).

---

### REQ-READYREAD-01 · P2 — `onReadyRead` must not use `sender()` for socket access

**EARS:** When data is available on the socket, the system shall read from `m_socket` directly rather than casting `sender()`.

**Root cause:** `qobject_cast<QTcpSocket*>(sender())` returns null if the slot is ever invoked without a signal context (e.g., direct call in tests). `m_socket` is already in scope.  
**File:** `NetworkManager.cpp:219`

**Test requirements:**
- Verify `onReadyRead()` processes data when called directly (not via signal).

- [x] Replace `QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender()); if (!socket) return;` and all `socket->` accesses with `m_socket->`.

---

### REQ-USERLIST-01 · P2 — User left must strip all standard prefix characters including `@`

**EARS:** When a user leaves a channel and the user list contains that nick with a prefix character, the system shall strip all standard IRC prefix characters (`~`, `&`, `@`, `%`, `+`) before comparing to find and remove the entry.

**Root cause:** The regex `[~%&+]` omits `@` (operator), the most common prefix. Ops who leave a channel remain in the user list permanently.  
**File:** `MainWindow.cpp:408`

**Test requirements:**
- Verify `@alice` is removed when `alice` parts.
- Verify `+alice` is removed when `alice` parts.
- Verify `alice` (no prefix) is removed when `alice` parts.

- [x] Change regex to `[~&@%+]` to include `@`.

---

### REQ-NICKCHANGE-01 · P2 — Nick change must update all tabs and fix user list entry

**EARS:** When a nick change is received, the system shall post the change message to every channel tab where the user was present, and shall update the user list entry to show the new nick with the correct prefix.

**Root cause:** `onUserChangedNick` only posts to `m_currentChannel` and replaces the user list item text with `"oldNick is now known as newNick"` — a sentence, not a nick.  
**File:** `MainWindow.cpp:371-380`

**Test requirements:**
- Verify nick-change message appears in every channel tab where the user was present.
- Verify the user list shows `newNick` (with preserved prefix) after the change, not a sentence.

- [x] Iterate all `IRCChannel*` via `m_network->channels()` and post the message to each whose backing `IRCChannel` contains the old nick.
- [x] In the user list update loop, find the item matching oldNick (stripping prefix chars), replace with `userPrefix() + newNick` (or just `newNick` if no prefix found).

---

### REQ-PING-01 · P2 — PING receipt must not flood the server tab

**EARS:** When a PING is received from the server, the system shall respond with PONG and shall not emit a user-visible message to the server tab.

**Root cause:** `handleCapCommand` PING handler still calls `emit serverChannelMessage("PONG: " + params[0])`, producing a line in the Server tab every 3 minutes.  
**File:** `NetworkManager.cpp:367-370`

**Test requirements:**
- Verify that receiving a PING from the server results in a PONG being sent and no message appearing in the server tab.

- [x] Remove `emit serverChannelMessage("PONG: " + params[0]);` from the PING handler.

---

### REQ-EXTRACTPREFIX-01 · P2 — Mode prefix extraction must correctly parse ISUPPORT PREFIX value

**EARS:** When ISUPPORT `PREFIX` is available, the system shall parse the `(mode_letters)symbols` format and use only the symbols portion to identify prefix characters on NAMES nicks.

**Root cause:** `extractModePrefix` default value `"@&*(op:s t: v: a"` is garbage. The function iterates over the entire PREFIX string (including letters and parentheses) when searching for a match, potentially stripping wrong characters from nicks.  
**File:** `NetworkManager.cpp:867-883`

**Test requirements:**
- Given `PREFIX=(ov)@+`, verify `extractModePrefix("@alice")` returns `@`.
- Given `PREFIX=(ov)@+`, verify `extractModePrefix("+bob")` returns `+`.
- Given `PREFIX=(ov)@+`, verify `extractModePrefix("charlie")` returns null/empty.
- Given no ISUPPORT PREFIX, verify default of `@%+` is used.

- [x] Parse `PREFIX=(letters)symbols`: extract the substring after `)` as the valid prefix chars.
- [x] Use a sensible default `"@%+"` when PREFIX is absent.
- [x] In `extractModePrefix(nick)`, check only whether `nick[0]` is in the symbols set.

---

## 3. MEDIUM — Incorrect Behaviour

### REQ-CAP-03 · P2 — CAP ACK must normalise cap names before storing

**EARS:** When a `CAP ACK` response is received, the system shall strip value suffixes (text after `=`) from each capability name before inserting into the active capability set.

**Root cause:** `sasl=PLAIN` is stored as `"sasl=PLAIN"`, making `m_activeCaps.contains("sasl")` false.  
**File:** `NetworkManager.cpp:712-719`

**Test requirements:**
- Given `CAP * ACK :sasl=PLAIN echo-message`, verify `m_activeCaps.contains("sasl")` and `m_activeCaps.contains("echo-message")` are both true.

- [x] For each cap token in the ACK list, strip everything from `=` onwards before inserting.
- [x] Also handle `-` prefix (cap disable): if cap starts with `-`, remove it from `m_activeCaps`.

---

### REQ-QUIT-02 · P2 — Quit reason must use correct field

**EARS:** When constructing a Quit message for display, the system shall use `reason` as the `m_text` field rather than the hardcoded string `"Quit"`.

- [ ] Already addressed in REQ-QUIT-01 above.

---

### REQ-MODE-02 · P2 — `ChannelTab::setMode` must not overwrite the topic bar

**EARS:** When a channel mode string is received, the system shall display it in the window title or status bar, and shall not overwrite the channel topic in the topic bar.

**User story:** As a user, I want the topic and mode to be shown separately so that a MODE change doesn't erase the channel topic.

**File:** `ChannelTab.cpp:49-51`

**Test requirements:**
- Verify that after setting a topic then receiving a MODE, the topic bar still shows the original topic.

- [ ] Remove `setTopic("Channel mode: " + mode)` from `ChannelTab::setMode`.
- [ ] Display mode in the parent `QTabWidget` tab title or a separate label, or discard it if no UI location exists yet.

---

### REQ-COPY-01 · P2 — Copy must place plain text (not HTML) on the clipboard

**EARS:** When the user selects Copy from the chat context menu, the system shall place the plain-text content of the message on the clipboard, with HTML markup removed.

**User story:** As a user, I want to copy chat messages and paste them into other applications without HTML tags appearing.

**File:** `ChatWidget.cpp:106-108`

**Test requirements:**
- Verify that copying a message containing `<b>nick</b>` produces clipboard text like `[12:34:56] nick: message`, not HTML.

- [ ] Strip HTML before placing on clipboard: use `QTextDocument doc; doc.setHtml(text); clipboard->setText(doc.toPlainText())`.

---

### REQ-SIZEHINT-01 · P2 — Chat row height must use forced document layout

**EARS:** The system shall compute each chat message row's display height by forcing `QTextDocument` layout before measuring, so that multi-line messages are not clipped.

**Root cause:** `block.layout()->lineCount()` before layout is forced returns 0. The fallback `fm.height() * 1.5` fires for most rows, producing inconsistent heights.  
**File:** `ChatWidget.cpp:40-64`

**Test requirements:**
- Verify a two-line wrapped message is rendered with a height ≥ 2× single-line height.
- Verify a single-line message renders at ~1× line height.

- [ ] Simplify `sizeHint` to:
  ```cpp
  QTextDocument doc;
  doc.setHtml(text);
  doc.setTextWidth(option.rect.width() > 0 ? option.rect.width() : 600);
  return QSize(qRound(doc.idealWidth()), qRound(doc.size().height()) + 4);
  ```
  `doc.size()` forces synchronous layout.

---

### REQ-ISUPPORT-01 · P2 — ISUPPORT loop must start at index 1

**EARS:** When a 005 RPL_ISUPPORT reply is received, the system shall parse tokens beginning at `params[1]`, skipping only the recipient nick at `params[0]`.

**Root cause:** Loop starts at `i = 2`, skipping `params[1]` which commonly holds `CHANTYPES` — the first and most important token.  
**File:** `NetworkManager.cpp:849`

**Test requirements:**
- Given `:srv 005 me CHANTYPES=# CASEMAPPING=rfc1459 :are supported`, verify `m_isupport["CHANTYPES"] == "#"`.

- [ ] Change `for (int i = 2; i < params.size(); ++i)` to `for (int i = 1; i < params.size(); ++i)`.
- [ ] Skip the trailing `:are supported by this server` token by checking `token.startsWith(':')` or by not processing the last param if it starts with `:`.

---

### REQ-NAMES-01 · P2 — Names received must not be gated on user-list visibility

**EARS:** When a NAMES reply is received for the current channel, the system shall update the user list regardless of whether the user-list panel is currently visible.

**Root cause:** `onNamesReceived` returns early if `!m_userList->isVisible()`. Hiding the panel then joining a channel leaves the user list permanently empty.  
**File:** `MainWindow.cpp:499`

**Test requirements:**
- Hide user list panel, join a channel, re-show panel — verify user list is populated.

- [ ] Remove the `|| !m_userList->isVisible()` guard from `onNamesReceived`.

---

### REQ-CHANNELS-01 · P2 — Channel list from `ServerDialog` must trim whitespace per entry

**EARS:** When the user enters a comma-separated channel list, the system shall trim leading and trailing whitespace from each channel name before using it.

**Root cause:** `"#a, #b".split(',')` → `["#a", " #b"]`. The space prefix makes `" #b"` an invalid JOIN target.  
**File:** `ServerDialog.cpp:71`

**Test requirements:**
- Verify `" #b".trimmed()` → `"#b"` is used in the JOIN command.

- [ ] Change `m_channelEdit->text().split(',', Qt::SkipEmptyParts)` to `.split(',').filter/map` with `.trimmed()` applied to each element, discarding empties after trim.

---

### REQ-RECONNECT-01 · P2 — Socket deletion during reconnect must not deliver signals to freed memory

**EARS:** If the socket is deleted during a reconnection attempt, the system shall block pending signals before deletion to prevent use-after-free.

**Root cause:** `m_socket->abort()` schedules signal delivery; `delete m_socket` immediately follows. Queued signals may fire on freed memory.  
**File:** `NetworkManager.cpp:40-54`

**Test requirements:**
- Verify calling `connectToServer` twice in rapid succession does not crash.

- [ ] Before `delete m_socket`, call `m_socket->blockSignals(true)` and `m_socket->disconnect()` (QObject disconnect, not IRC disconnect).

---

### REQ-CHANNELMODEL-01 · P3 — Dead `IRCChannelModel` must be removed or wired to the sidebar

**EARS:** The system shall not instantiate objects that are never used as data sources for any view.

**Root cause:** `m_channelModel` (`IRCChannelModel`) is constructed but never set on `m_channelList` (a `QListWidget`). Channel names are added imperatively via `addItem`, not via the model.  
**File:** `MainWindow.cpp:93`, `MainWindow.h:71`

**Test requirements:**
- Either: verify channel sidebar uses `IRCChannelModel` as its data source.
- Or: verify `m_channelModel` declaration and construction are absent.

- [ ] Either wire `m_channelModel` to `m_channelList` (replace `QListWidget` with `QListView`), **or** delete `m_channelModel` entirely and keep imperative `QListWidget` management.

---

### REQ-CLI-01 · P3 — Parsed CLI arguments must be used

**EARS:** When the user supplies `--host`, `--port`, `--nick`, or `--tls` on the command line, the system shall pre-populate the connection dialog or connect directly without showing the dialog.

**Root cause:** All CLI options are defined and parsed but `parser.value(...)` is never called. `MainWindow` always shows the connection dialog.  
**File:** `src/main.cpp:13-30`

**Test requirements:**
- Running `qwenirc --host irc.example.com --port 6667 --nick testuser` shall pre-fill those fields in the dialog.

- [ ] After `parser.process(app)`, read values and pass them to `MainWindow` constructor (or a separate factory method).

---

### REQ-SETMODE-01 · P3 — `setMode` topic-bar abuse

Already covered under REQ-MODE-02.

---

## 4. LOW

### REQ-ESCAPEHTML-01 · P4 — Remove `escapeHTML` wrapper

**EARS:** The system shall not maintain a static wrapper method that merely delegates to `QString::toHtmlEscaped()`.

- [ ] Delete `IRCMessage::escapeHTML` declaration from `IRCMessage.h` and implementation from `IRCMessage.cpp`.
- [ ] Replace all `escapeHTML(x)` calls with `x.toHtmlEscaped()`.

---

### REQ-DUPMETHOD-01 · P4 — Remove duplicate `ChatWidget::setChannel`

**EARS:** The system shall expose a single method for setting the channel name on `ChatWidget`.

- [ ] Remove `setChannel(const QString&)` from `ChatWidget`; all callers already use `setChannelName`.

---

### REQ-USERPREFIX-01 · P4 — `IRCUser::userPrefix` must store display symbol, not mode letter

**EARS:** The system shall store the prefix display character (`@`, `+`, `%`) in `IRCUser::userPrefix`, not the mode letter (`o`, `v`, `h`).

**Root cause:** `applyMode` (once fixed per REQ-MODE-01) must map mode letters to symbols using the PREFIX ISUPPORT mapping before calling `setUserPrefix`.

- [ ] Ensure the mapping from mode letter → symbol happens in `NetworkManager::handleMode` or `IRCChannel::applyMode`, not in `IRCUserModel::data`.

---

### REQ-OPERATOR-01 · P4 — `IRCMessage::operator==` must include timestamp

**EARS:** When two `IRCMessage` objects are compared for equality, the system shall include the timestamp field in the comparison.

- [ ] Add `&& m_timestamp == other.m_timestamp` to `IRCMessage::operator==`.

---

### REQ-LOGGING-01 · P4 — `Q_LOGGING_CATEGORY` must be used or removed

**EARS:** The system shall not declare a logging category that is never referenced.

- [ ] Either add `qCDebug(logIRC)` calls at key parse/send points, or remove `Q_LOGGING_CATEGORY(logIRC, "qwenirc.irc")`.

---

### REQ-AUTOUIC-01 · P4 — Remove unused `CMAKE_AUTOUIC`

**EARS:** The build system shall not enable processing stages for file types that do not exist in the project.

- [ ] Remove `set(CMAKE_AUTOUIC ON)` from `CMakeLists.txt` (no `.ui` files present).

---

### REQ-DEADMEMBER-01 · P4 — Remove `m_channel` dead member from `ServerDialog`

- [ ] Remove `QString m_channel` from `ServerDialog.h` and its initialiser from the constructor.

---

### REQ-CHANNELREGEX-01 · P4 — Channel validator regex must allow non-word characters

**EARS:** When validating channel names, the system shall allow any character that IRC permits in channel names, not just `\w` (word chars).

- [ ] Replace or relax the `QRegularExpressionValidator` to accept `-`, `.`, and other common channel name characters, or remove the validator and rely on server rejection.

---

### REQ-CLOSETAB-01 · P4 — `ChannelTab::close` must remove itself from `QTabWidget` before deletion

**EARS:** When a channel tab is closed, the system shall remove it from the parent `QTabWidget` before calling `deleteLater`.

- [ ] In `ChannelTab::close()`, call `parentWidget()` cast to find the `QTabWidget` and `removeTab` before `deleteLater()`.

---

### REQ-WAITINGCAPS-01 · P4 — Remove unused `m_waitingCaps` field

**EARS:** The system shall not retain member variables that are set but never read.

- [ ] Delete `m_waitingCaps` from header and all assignment sites.

---

## 5. Testing Infrastructure

### REQ-TEST-01 · P3 — Unit tests for IRC line parser

**EARS:** The system shall include automated tests that verify `NetworkManager::parseMessage` correctly parses prefix, command, and parameter fields for both prefixed and non-prefixed lines, including IRCv3 message tags.

**User story:** As a developer, I want parser tests so that numeric-reply regressions are caught before they reach users.

- [ ] Add `tests/` directory with `CMakeLists.txt` using `find_package(Qt6 REQUIRED COMPONENTS Test)`.
- [ ] Test: `:server 353 me = #chan :@op +voice user` → channel `"#chan"`, users `["op","voice","user"]` with correct prefixes.
- [ ] Test: `@time=2024-01-01T12:00:00Z :srv 332 me #chan :topic` → timestamp parsed, channel `"#chan"`, topic `"topic"`.
- [ ] Test: `:srv PING :pingtoken` → `PONG :pingtoken` sent, no user message emitted.
- [ ] Test: `CAP * LS :sasl server-time echo-message` → intersection computed correctly, `CAP REQ` sent with correct cap list.

### REQ-TEST-02 · P3 — Unit tests for IRCUserModel

- [ ] Test: `addUser` with duplicate nick (case-insensitive) does not insert duplicate.
- [ ] Test: `removeUser("alice")` removes `@alice` from display list.
- [ ] Test: `setUsers` populates model and emits `modelReset`.

### REQ-TEST-03 · P3 — Unit tests for IRCMessage rendering

- [ ] Test: `coloredText()` for `MessageType::Message` with nick `<b>nick</b>` — verify `<b>` is HTML-escaped in output.
- [ ] Test: `formattedText()` for `MessageType::Quit` with non-empty reason includes the reason.
- [ ] Test: `formattedText()` for `MessageType::TopicSet` uses `%1` placeholder correctly.

---

## Audit Status

After all items above are implemented, a fresh audit will be performed to verify correctness and identify any remaining issues.
