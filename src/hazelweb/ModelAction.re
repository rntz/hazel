open Sexplib.Std;

[@deriving sexp]
type shift_history_info = {
  group_id: int,
  elt_id: int,
  call_by_mouseenter: bool,
};
[@deriving sexp]
type group_id = int;
[@deriving sexp]
type t =
  | EditAction(Action.t)
  | MoveAction(MoveInput.t)
  | ToggleLeftSidebar
  | ToggleRightSidebar
  | LoadExample(Examples.id)
  | LoadCardstack(int)
  | NextCard
  | PrevCard
  | UpdateSettings(Settings.update)
  | SelectHoleInstance(HoleInstance.t)
  | SelectCaseBranch(CursorPath.steps, int)
  | InvalidVar(string)
  | FocusCell
  | BlurCell
  | Redo
  | Undo
  | ShiftHistory(shift_history_info)
  | ShiftWhenScroll
  | ToggleHistoryGroup(group_id)
  | ToggleHiddenHistoryAll
  | TogglePreviewOnHover
  | UpdateFontMetrics(FontMetrics.t)
  | UpdateIsMac(bool);
