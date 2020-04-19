open Sexplib.Std;

[@deriving sexp]
type t =
  | Collapsed
  | HoleLabel
  | FreeLivelitLabel
  | Delim
  | EmptyHole(bool, NodeInstance.t)
  | NonEmptyHole(ErrStatus.HoleReason.t, NodeInstance.t)
  | VarHole(VarErrStatus.HoleReason.t, NodeInstance.t)
  | FreeLivelit(LivelitName.t, NodeInstance.t)
  | FailedCastDelim
  | FailedCastDecoration
  | CastDecoration;