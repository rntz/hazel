open Pretty;

type t = Doc.t(UHAnnot.t);
type splices = SpliceMap.t(t);
type with_splices = (t, splices);

let map_root = (f: t => t, (doc, splices): with_splices): with_splices => (
  f(doc),
  splices,
);

type memoization_value('a) = {
  mutable inline_true: option('a),
  mutable inline_false: option('a),
};

let memoize =
    (f: (~memoize: bool, ~enforce_inline: bool, 'k) => 'v)
    : ((~memoize: bool, ~enforce_inline: bool, 'k) => 'v) => {
  let table: WeakMap.t('k, memoization_value('v)) = WeakMap.mk();
  (~memoize: bool, ~enforce_inline: bool, k: 'k) => (
    if (!memoize) {
      f(~memoize, ~enforce_inline, k);
    } else {
      switch (WeakMap.get(table, k)) {
      | None =>
        let v = f(~memoize, ~enforce_inline, k);
        let m =
          if (enforce_inline) {
            {inline_true: Some(v), inline_false: None};
          } else {
            {inline_false: Some(v), inline_true: None};
          };
        let _ = WeakMap.set(table, k, m);
        v;
      | Some((m: memoization_value('v))) =>
        if (enforce_inline) {
          switch (m.inline_true) {
          | Some(v) => v
          | None =>
            let v = f(~memoize, ~enforce_inline, k);
            m.inline_true = Some(v);
            v;
          };
        } else {
          switch (m.inline_false) {
          | Some(v) => v
          | None =>
            let v = f(~memoize, ~enforce_inline, k);
            m.inline_false = Some(v);
            v;
          };
        }
      };
    }: 'v
  );
};

let empty_: t = Doc.empty();
let space_: t = Doc.space();

let indent_and_align_ = (doc: t): t =>
  Doc.(hcat(annot(UHAnnot.Indent, indent()), align(doc)));

module Delim = {
  let mk = (~index: int, delim_text: string): t =>
    Doc.annot(
      UHAnnot.mk_Token(
        ~len=StringUtil.utf8_length(delim_text),
        ~shape=Delim(index),
        (),
      ),
      Doc.text(delim_text),
    );

  let empty_hole_doc = (hole_lbl: string): t => {
    let len = hole_lbl |> StringUtil.utf8_length;
    Doc.text(hole_lbl)
    |> Doc.annot(UHAnnot.HoleLabel({len: len}))
    |> Doc.annot(UHAnnot.mk_Token(~shape=Delim(0), ~len, ()));
  };

  let open_List = (): t => mk(~index=0, "[");
  let close_List = (): t => mk(~index=1, "]");

  let open_Parenthesized = (): t => mk(~index=0, "(");
  let close_Parenthesized = (): t => mk(~index=1, ")");

  let open_Inj = (inj_side: InjSide.t): t =>
    mk(~index=0, "inj[" ++ InjSide.to_string(inj_side) ++ "](");
  let close_Inj = (): t => mk(~index=1, ")");

  let sym_Lam = (): t => mk(~index=0, UnicodeConstants.lamSym);
  let colon_Lam = (): t => mk(~index=1, ":");
  let open_Lam = (): t => mk(~index=2, ".{");
  let close_Lam = (): t => mk(~index=3, "}");

  let open_Case = (): t => mk(~index=0, "case");
  let close_Case = (): t => mk(~index=1, "end");
  let close_Case_ann = (): t => mk(~index=1, "end :");

  let bar_Rule = (): t => mk(~index=0, "|");
  let arrow_Rule = (): t => mk(~index=1, "=>");

  let let_LetLine = (): t => mk(~index=0, "let");
  let colon_LetLine = (): t => mk(~index=1, ":");
  let eq_LetLine = (): t => mk(~index=2, "=");
  let in_LetLine = (): t => mk(~index=3, "in");

  let abbrev_AbbrevLine = () => mk(~index=0, "abbrev");
  let eq_AbbrevLine = () => mk(~index=1, "=");
  let in_AbbrevLine = () => mk(~index=2, "in");
};

let annot_Indent: t => t = Doc.annot(UHAnnot.Indent);
let annot_Padding = (d: t): t =>
  switch (d.doc) {
  | Text("") => d
  | _ => Doc.annot(UHAnnot.Padding, d)
  };
let annot_Tessera: t => t = Doc.annot(UHAnnot.Tessera);
let annot_OpenChild = (~is_enclosed=true, ~is_inline: bool): (t => t) =>
  Doc.annot(UHAnnot.mk_OpenChild(~is_enclosed, ~is_inline, ()));
let annot_ClosedChild = (~is_inline: bool): (t => t) =>
  Doc.annot(UHAnnot.mk_ClosedChild(~is_inline, ()));
let annot_Step = (step: int): (t => t) => Doc.annot(UHAnnot.Step(step));
let annot_Var =
    (~sort: TermSort.t, ~err: ErrStatus.t=NotInHole, ~verr: VarErrStatus.t)
    : (t => t) =>
  Doc.annot(
    UHAnnot.mk_Term(~sort, ~shape=TermShape.mk_Var(~err, ~verr, ()), ()),
  );
let annot_Operand = (~sort: TermSort.t, ~err: ErrStatus.t=NotInHole): (t => t) =>
  Doc.annot(
    UHAnnot.mk_Term(~sort, ~shape=TermShape.mk_Operand(~err, ()), ()),
  );
let annot_Case = (~err: CaseErrStatus.t): (t => t) =>
  Doc.annot(UHAnnot.mk_Term(~sort=Exp, ~shape=Case({err: err}), ()));
let annot_Invalid = (~sort: TermSort.t): (t => t) =>
  Doc.annot(UHAnnot.mk_Term(~sort, ~shape=TermShape.Invalid, ()));

let annot_FreeLivelit =
  Doc.annot(UHAnnot.mk_Term(~sort=Exp, ~shape=TermShape.FreeLivelit, ()));
let annot_LivelitExpression = Doc.annot(UHAnnot.LivelitExpression);

let indent_and_align = (d: t): t =>
  Doc.(hcats([indent() |> annot_Indent, align(d)]));

let mk_text = (~start_index=0, s: string): t =>
  Doc.annot(
    UHAnnot.mk_Token(
      ~shape=Text(start_index),
      ~len=StringUtil.utf8_length(s),
      (),
    ),
    Doc.text(s),
  );

let mk_op = (op_text: string): t =>
  Doc.annot(
    UHAnnot.mk_Token(~len=StringUtil.utf8_length(op_text), ~shape=Op, ()),
    Doc.text(op_text),
  );

let mk_space_op: t = Doc.annot(UHAnnot.SpaceOp, space_);

let user_newline: t =
  Doc.(
    hcats([
      space_ |> annot_Padding,
      text(UnicodeConstants.user_newline) |> annot(UHAnnot.UserNewline),
    ])
  );

type formattable_child = (~enforce_inline: bool) => t;
type formatted_child =
  | UserNewline(t)
  | EnforcedInline(t)
  | Unformatted(formattable_child);

let pad_child =
    (
      ~is_open: bool,
      ~inline_padding: (t, t)=(empty_, empty_),
      child: formatted_child,
    )
    : t => {
  open Doc;
  // TODO review child annotation and simplify if possible
  let annot_child =
    is_open ? annot_OpenChild(~is_enclosed=true) : annot_ClosedChild;
  let inline_choice = child_doc => {
    let (left, right) = inline_padding;
    let lpadding = left == empty_ ? [] : [left |> annot_Padding];
    let rpadding = right == empty_ ? [] : [right |> annot_Padding];
    hcats([
      hcats(List.concat([lpadding, [child_doc], rpadding]))
      |> annot_child(~is_inline=true),
    ]);
  };
  let para_choice = child_doc =>
    child_doc |> indent_and_align |> annot_child(~is_inline=false);
  switch (child) {
  | EnforcedInline(child_doc) => inline_choice(child_doc)
  | UserNewline(child_doc) =>
    hcats([user_newline, linebreak(), para_choice(child_doc), linebreak()])
  | Unformatted(formattable_child) =>
    choices([
      inline_choice(formattable_child(~enforce_inline=true)),
      hcats([
        linebreak(),
        para_choice(formattable_child(~enforce_inline=false)),
        linebreak(),
      ]),
    ])
  };
};

let pad_open_child: (~inline_padding: (t, t)=?, formatted_child) => t =
  pad_child(~is_open=true);
let pad_closed_child: (~inline_padding: (t, t)=?, formatted_child) => t =
  pad_child(~is_open=false);

let pad_left_delimited_child =
    (~is_open: bool, ~inline_padding: t=empty_, child: formatted_child): t => {
  open Doc;
  let annot_child =
    // TODO is_enclosed flag is not right
    is_open ? annot_OpenChild(~is_enclosed=true) : annot_ClosedChild;
  let inline_choice = child_doc => {
    let lpadding =
      inline_padding == empty_ ? [] : [inline_padding |> annot_Padding];
    hcats(lpadding @ [child_doc]) |> annot_child(~is_inline=true);
  };
  let para_choice = child_doc =>
    child_doc |> indent_and_align |> annot_child(~is_inline=false);
  switch (child) {
  | EnforcedInline(child_doc) => inline_choice(child_doc)
  | UserNewline(child_doc) =>
    hcats([user_newline, linebreak(), para_choice(child_doc)])
  | Unformatted(formattable_child) =>
    choices([
      inline_choice(formattable_child(~enforce_inline=true)),
      hcats([
        linebreak(),
        para_choice(formattable_child(~enforce_inline=false)),
      ]),
    ])
  };
};

type binOp_handler('operand, 'operator) =
  (Skel.t('operator) => t, Seq.t('operand, 'operator), Skel.t('operator)) =>
  option(t);

let def_bosh = (_, _, _) => None;

let mk_Unit = (): t =>
  Delim.mk(~index=0, "()") |> annot_Tessera |> annot_Operand(~sort=Typ);

let mk_Bool = (): t =>
  Delim.mk(~index=0, "Bool") |> annot_Tessera |> annot_Operand(~sort=Typ);

let mk_Int = (): t =>
  Delim.mk(~index=0, "Int") |> annot_Tessera |> annot_Operand(~sort=Typ);

let mk_Float = (): t =>
  Delim.mk(~index=0, "Float") |> annot_Operand(~sort=Typ);

let hole_lbl = (u: MetaVar.t): string => string_of_int(u);
let hole_inst_lbl = (u: MetaVar.t, i: MetaVarInst.t): string =>
  StringUtil.cat([string_of_int(u), ":", string_of_int(i)]);

let mk_EmptyHole = (~sort: TermSort.t, hole_lbl: string): t =>
  Delim.empty_hole_doc(hole_lbl) |> annot_Tessera |> annot_Operand(~sort);

let mk_Wild = (~err: ErrStatus.t): t =>
  Delim.mk(~index=0, "_") |> annot_Tessera |> annot_Operand(~sort=Pat, ~err);

let mk_InvalidText = (~sort: TermSort.t, t: string): t =>
  mk_text(t) |> annot_Tessera |> annot_Invalid(~sort);

let mk_Var =
    (~sort: TermSort.t, ~err: ErrStatus.t, ~verr: VarErrStatus.t, x: Var.t): t =>
  mk_text(x) |> annot_Tessera |> annot_Var(~sort, ~err, ~verr);

let mk_IntLit = (~sort: TermSort.t, ~err: ErrStatus.t, n: string): t =>
  mk_text(n) |> annot_Tessera |> annot_Operand(~sort, ~err);

let mk_FloatLit = (~sort: TermSort.t, ~err: ErrStatus.t, f: string): t =>
  mk_text(f) |> annot_Tessera |> annot_Operand(~sort, ~err);

let mk_BoolLit = (~sort: TermSort.t, ~err: ErrStatus.t, b: bool): t =>
  mk_text(string_of_bool(b)) |> annot_Tessera |> annot_Operand(~sort, ~err);

let mk_ListNil = (~sort: TermSort.t, ~err: ErrStatus.t, ()): t =>
  Delim.mk(~index=0, "[]") |> annot_Tessera |> annot_Operand(~sort, ~err);

let mk_Parenthesized = (~sort: TermSort.t, body: formatted_child): t => {
  let open_group = Delim.open_Parenthesized() |> annot_Tessera;
  let close_group = Delim.close_Parenthesized() |> annot_Tessera;
  Doc.hcats([open_group, body |> pad_open_child, close_group])
  |> annot_Operand(~sort);
};

let mk_List = (body: formatted_child): t => {
  let open_group = Delim.open_List() |> annot_Tessera;
  let close_group = Delim.close_List() |> annot_Tessera;
  Doc.hcats([open_group, body |> pad_open_child, close_group])
  |> annot_Operand(~sort=Typ);
};

let mk_Inj =
    (
      ~sort: TermSort.t,
      ~err: ErrStatus.t,
      ~inj_side: InjSide.t,
      body: formatted_child,
    )
    : t => {
  let open_group = Delim.open_Inj(inj_side) |> annot_Tessera;
  let close_group = Delim.close_Inj() |> annot_Tessera;
  Doc.hcats([open_group, body |> pad_open_child, close_group])
  |> annot_Operand(~sort, ~err);
};

let mk_Lam =
    (
      ~err: ErrStatus.t,
      p: formatted_child,
      ann: option(formatted_child),
      body: formatted_child,
    )
    : t => {
  let open_group = {
    let lam_delim = Delim.sym_Lam();
    let open_delim = Delim.open_Lam();
    let doc =
      switch (ann) {
      | None => Doc.hcats([lam_delim, p |> pad_closed_child, open_delim])
      | Some(ann) =>
        let colon_delim = Delim.colon_Lam();
        Doc.hcats([
          lam_delim,
          p |> pad_closed_child,
          colon_delim,
          ann |> pad_closed_child,
          open_delim,
        ]);
      };
    doc |> annot_Tessera;
  };
  let close_group = Delim.close_Lam() |> annot_Tessera;
  Doc.hcats([open_group, body |> pad_open_child, close_group])
  |> annot_Operand(~sort=Exp, ~err);
};

let mk_Case =
    (~err: CaseErrStatus.t, scrut: formatted_child, rules: list(t)): t => {
  let open_group = Delim.open_Case() |> annot_Tessera;
  let close_group = Delim.close_Case() |> annot_Tessera;
  Doc.(
    vseps([
      hcats([
        open_group,
        scrut
        |> pad_left_delimited_child(~is_open=true, ~inline_padding=space_),
      ]),
      // TODO undo open child hack when fixing case indicator
      annot_OpenChild(~is_inline=false, vseps(rules)),
      close_group,
    ])
  )
  |> annot_Case(~err);
};

let mk_Rule = (p: formatted_child, clause: formatted_child): t => {
  let delim_group =
    Doc.hcats([
      Delim.bar_Rule(),
      p |> pad_closed_child(~inline_padding=(space_, space_)),
      Delim.arrow_Rule(),
    ])
    |> annot_Tessera;
  Doc.hcats([
    delim_group,
    clause |> pad_left_delimited_child(~is_open=true, ~inline_padding=space_),
  ])
  |> Doc.annot(UHAnnot.mk_Term(~sort=Exp, ~shape=Rule, ()));
};

let mk_FreeLivelit = (lln: LivelitName.t): t =>
  annot_FreeLivelit(annot_Tessera(mk_text(lln)));

let mk_ApLivelit = (llname: LivelitName.t): t => {
  Doc.annot(
    UHAnnot.mk_Term(~sort=Exp, ~shape=ApLivelit, ()),
    annot_Tessera(mk_text(llname)),
  );
};

let mk_LetLine =
    (p: formatted_child, ann: option(formatted_child), def: formatted_child)
    : t => {
  let open_group = {
    let let_delim = Delim.let_LetLine();
    let eq_delim = Delim.eq_LetLine();
    let doc =
      switch (ann) {
      | None =>
        Doc.hcats([
          let_delim,
          p |> pad_closed_child(~inline_padding=(space_, space_)),
          eq_delim,
        ])
      | Some(ann) =>
        let colon_delim = Delim.colon_LetLine();
        Doc.hcats([
          let_delim,
          p |> pad_closed_child(~inline_padding=(space_, space_)),
          colon_delim,
          ann |> pad_closed_child(~inline_padding=(space_, space_)),
          eq_delim,
        ]);
      };
    doc |> annot_Tessera;
  };
  let close_group = Delim.in_LetLine() |> annot_Tessera;
  Doc.hcats([
    open_group,
    def |> pad_open_child(~inline_padding=(space_, space_)),
    close_group,
  ]);
};

let mk_AbbrevLine =
    (
      lln_new: LivelitName.t,
      lln_old: LivelitName.t,
      args: list(formatted_child),
    )
    : t => {
  let open_group = {
    let abbrev_delim = Delim.abbrev_AbbrevLine();
    let eq_delim = Delim.eq_AbbrevLine();
    let lln_new_doc = Doc.hseps([abbrev_delim, mk_text(lln_new), eq_delim]);
    lln_new_doc |> annot_Tessera;
  };
  let inline_choice = {
    let args =
      args
      |> List.map(
           fun
           | UserNewline(_) => Doc.fail()
           | EnforcedInline(arg) => arg
           | Unformatted(arg) => arg(~enforce_inline=true),
         );
    Doc.(hcats([space_, ...args] @ [space_]));
  };
  let old_ll_group =
    Doc.hseps([
      mk_text(~start_index=LivelitName.length(lln_new) + 1, lln_old),
      inline_choice,
    ])
    |> annot_Tessera;
  let close_group = Delim.in_AbbrevLine() |> annot_Tessera;
  Doc.hseps([open_group, old_ll_group, close_group]);
};

let pad_operator =
    (~inline_padding as (left, right): (t, t), operator: t): t => {
  open Doc;
  let ldoc = left == empty_ ? empty_ : left |> annot_Padding;
  let rdoc = right == empty_ ? empty_ : right |> annot_Padding;
  choices([
    hcats([ldoc, operator, rdoc]),
    hcats([linebreak(), operator, rdoc]),
  ]);
};

let rec mk_BinOp =
        (
          ~sort: TermSort.t,
          ~mk_operand: (~enforce_inline: bool, 'operand) => t,
          ~mk_operator: 'operator => t,
          ~inline_padding_of_operator: 'operator => (t, t),
          ~enforce_inline: bool,
          ~check_livelit_skel:
             (Seq.t('operand, 'operator), Skel.t('operator)) =>
             option(LivelitUtil.llctordata)=(_, _) => None,
          ~seq: Seq.t('operand, 'operator),
          skel: Skel.t('operator),
        )
        : t => {
  let go =
    mk_BinOp(
      ~sort,
      ~mk_operand,
      ~mk_operator,
      ~inline_padding_of_operator,
      ~seq,
    );

  switch (check_livelit_skel(seq, skel)) {
  | Some(ApLivelitData(llu, base_llname, llname, model, _)) =>
    let ctx = Livelits.initial_livelit_view_ctx;
    switch (VarMap.lookup(ctx, base_llname)) {
    | None => failwith("livelit " ++ base_llname ++ " not found")
    | Some((_, shape_fn)) =>
      let shape = shape_fn(model);
      let hd_step = Skel.leftmost_tm_index(skel);
      let llexp = annot_LivelitExpression(go(~enforce_inline, skel));
      let annot_LivelitView =
        Doc.annot(
          UHAnnot.LivelitView({
            llu,
            base_llname,
            llname,
            shape,
            model,
            hd_step,
          }),
        );
      switch (shape) {
      | Inline(width) =>
        let spaceholder = Doc.text(StringUtil.replicat(width, Unicode.nbsp));
        Doc.hsep(llexp, annot_LivelitView(spaceholder));
      | MultiLine(height) =>
        if (enforce_inline) {
          Doc.fail();
        } else {
          let spaceholder =
            Doc.hcats(ListUtil.replicate(height, Doc.linebreak()));
          Doc.vsep(llexp, annot_LivelitView(spaceholder));
        }
      };
    };
  | _ =>
    switch (skel) {
    | Placeholder(n) =>
      let operand = Seq.nth_operand(n, seq);
      annot_Step(n, mk_operand(~enforce_inline, operand));
    | BinOp(err, op, skel1, skel2) =>
      let op_index = Skel.rightmost_tm_index(skel1) + Seq.length(seq);
      let (lpadding, rpadding) = {
        let (l, r) = inline_padding_of_operator(op);
        (
          l == empty_ ? [] : [annot_Padding(l)],
          r == empty_ ? [] : [annot_Padding(r)],
        );
      };
      let op = annot_Tessera(annot_Step(op_index, mk_operator(op)));
      let skel1 = go(skel1);
      let skel2 = go(skel2);
      let annot_unenclosed_OpenChild = annot_OpenChild(~is_enclosed=false);
      let inline_choice =
        Doc.(
          hcats([
            annot_unenclosed_OpenChild(
              ~is_inline=true,
              hcats([skel1(~enforce_inline=true), ...lpadding]),
            ),
            op,
            annot_unenclosed_OpenChild(
              ~is_inline=true,
              hcats(rpadding @ [skel2(~enforce_inline=true)]),
            ),
          ])
        );
      let multiline_choice =
        Doc.(
          vsep(
            annot_unenclosed_OpenChild(
              ~is_inline=false,
              align(skel1(~enforce_inline=false)),
            ),
            hcat(
              op,
              // TODO need to have a choice here for multiline vs not
              annot_unenclosed_OpenChild(
                ~is_inline=false,
                hcats(rpadding @ [align(skel2(~enforce_inline=false))]),
              ),
            ),
          )
        );
      let choices =
        enforce_inline
          ? inline_choice : Doc.choice(inline_choice, multiline_choice);
      Doc.annot(
        UHAnnot.mk_Term(~sort, ~shape=BinOp({err, op_index}), ()),
        choices,
      );
    }
  };
};

let mk_NTuple =
    (
      ~sort: TermSort.t,
      ~get_tuple_elements: Skel.t('operator) => list(Skel.t('operator)),
      ~mk_operand: (~enforce_inline: bool, 'operand) => t,
      ~mk_operator: 'operator => t,
      ~inline_padding_of_operator: 'operator => (t, t),
      ~enforce_inline: bool,
      ~check_livelit_skel:
         (Seq.t('operand, 'operator), Skel.t('operator)) =>
         option(LivelitUtil.llctordata)=(_, _) => None,
      OpSeq(skel, seq): OpSeq.t('operand, 'operator),
    )
    : t => {
  let mk_BinOp =
    mk_BinOp(
      ~sort,
      ~mk_operand,
      ~mk_operator,
      ~inline_padding_of_operator,
      ~check_livelit_skel,
      ~seq,
    );

  switch (get_tuple_elements(skel)) {
  | [] => failwith(__LOC__ ++ ": found empty tuple")
  | [singleton] => mk_BinOp(~enforce_inline, singleton)
  | [hd, ...tl] =>
    let err =
      switch (skel) {
      | Placeholder(_) => assert(false)
      | BinOp(err, _, _, _) => err
      };
    let hd_doc = (~enforce_inline: bool) =>
      // TODO need to relax is_inline
      annot_OpenChild(
        ~is_inline=enforce_inline,
        ~is_enclosed=false,
        mk_BinOp(~enforce_inline, hd),
      );
    let comma_doc = (step: int) => annot_Step(step, mk_op(","));
    let (inline_choice, comma_indices) =
      tl
      |> ListUtil.fold_left_i(
           ((tuple, comma_indices), (i, elem)) => {
             let comma_index =
               Skel.leftmost_tm_index(elem) - 1 + Seq.length(seq);
             let elem_doc = mk_BinOp(~enforce_inline=true, elem);
             let doc =
               Doc.hcats([
                 tuple,
                 annot_Tessera(comma_doc(comma_index)),
                 annot_OpenChild(
                   ~is_enclosed=i == List.length(tl) - 1,
                   ~is_inline=true,
                   Doc.hcat(annot_Padding(space_), elem_doc),
                 ),
               ]);
             (doc, [comma_index, ...comma_indices]);
           },
           (hd_doc(~enforce_inline=true), []),
         );
    let multiline_choice =
      tl
      |> ListUtil.fold_left_i(
           (tuple, (i, elem)) => {
             let comma_index =
               Skel.leftmost_tm_index(elem) - 1 + Seq.length(seq);
             let elem_doc = mk_BinOp(~enforce_inline=false, elem);
             Doc.(
               vsep(
                 tuple,
                 hcat(
                   annot_Tessera(comma_doc(comma_index)),
                   // TODO need to have a choice here for multiline vs not
                   annot_OpenChild(
                     ~is_enclosed=i == List.length(tl) - 1,
                     ~is_inline=false,
                     hcat(annot_Padding(space_), align(elem_doc)),
                   ),
                 ),
               )
             );
           },
           hd_doc(~enforce_inline=false),
         );
    let choices =
      enforce_inline
        ? inline_choice : Doc.choice(inline_choice, multiline_choice);
    Doc.annot(
      UHAnnot.mk_Term(~sort, ~shape=NTuple({comma_indices, err}), ()),
      choices,
    );
  };
};
