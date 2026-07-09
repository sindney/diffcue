## MODIFIED Requirements

### Requirement: Cue list and jump-to-cue

The toolbar's cue counter (`Cues: N`) SHALL be rendered every frame, including when the count is zero (`Cues: 0`). Hovering the cue counter with the mouse for a short dwell (no click required) SHALL open the cue list popup (`cue_dropdown`); the popup SHALL close when the mouse leaves both the button and the popup. The list SHALL show every cue as `file:line - text`. Clicking an entry SHALL open that file in the diff viewer and scroll the viewer so the cue's line is centered. The previous plain-text hover tooltip ("Click to list all cues.") SHALL be removed. The cue counter SHALL NOT open the popup on click; hover is the sole trigger.

#### Scenario: Hover opens the cue list

- **WHEN** the user hovers the `Cues: 3` button and holds the mouse still for ~300 ms
- **THEN** the `cue_dropdown` popup appears under the button listing all three cues as `file:line - text` entries

#### Scenario: Click on a cue jumps and scrolls

- **WHEN** the user is viewing `a.cpp`, the cue list is open, and the user clicks the entry for `b.h:7 - missing guard`
- **THEN** the diff viewer loads `b.h` and scrolls so line 7 is centered in the viewport, and the cue marker on line 7 is rendered

#### Scenario: Zero cues still renders the button

- **WHEN** the cue store is empty
- **THEN** the toolbar still renders `Cues: 0`, and hovering it opens an empty `cue_dropdown` popup (or a disabled "No cues" row) rather than hiding the button

#### Scenario: Mouse leaves the popup area

- **WHEN** the cue list is open and the user moves the mouse off both the `Cues: N` button and the `cue_dropdown` popup
- **THEN** the popup closes without selecting any entry

## ADDED Requirements

### Requirement: Clear all cues confirmation

The toolbar SHALL provide a `Clear` button that, when clicked, opens a centered modal popup titled `Clear Cues?` showing the cue count and two buttons: `Clear (Enter)` and `Cancel (Esc)`. Clicking `Clear (Enter)` or pressing **Enter** while the modal is open SHALL remove every cue from the store and close the modal. Clicking `Cancel (Esc)` or pressing **Esc** while the modal is open SHALL close the modal without modifying any cue. The button labels SHALL include their shortcut hints in parentheses. The modal SHALL NOT require a mouse interaction to confirm or cancel — both actions SHALL be reachable from the keyboard alone.

#### Scenario: Confirm with Enter

- **WHEN** the `Clear Cues?` modal is open with 5 cues and the user presses **Enter**
- **THEN** all 5 cues are deleted from the store and the modal closes

#### Scenario: Cancel with Esc

- **WHEN** the `Clear Cues?` modal is open with 5 cues and the user presses **Esc**
- **THEN** no cue is deleted and the modal closes

#### Scenario: Confirm with the mouse

- **WHEN** the `Clear Cues?` modal is open and the user clicks the `Clear (Enter)` button
- **THEN** all cues are deleted and the modal closes (identical to the Enter-key path)

#### Scenario: Button labels show shortcuts

- **WHEN** the `Clear Cues?` modal renders
- **THEN** the confirm button label contains `Enter` and the cancel button label contains `Esc`, so the keyboard shortcuts are discoverable without leaving the modal
