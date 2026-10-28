(Dialog codex_native_assistant
 (Components
  (Label Heading)
  (TextArea History)
  (TextArea Prompt)
  (PushButton ReadModel)
  (PushButton Run)
  (PushButton Close)
 )
 (Resources
  (.Label "Codex Native Assistant")
  (.Resizeable True)
  (Heading.Label "Codex AI / Template modeling / Auto execute")
  (History.Columns 64)
  (History.Rows 16)
  (History.MinRows 8)
  (Prompt.Columns 64)
  (Prompt.Rows 5)
  (Prompt.MinRows 5)
  (ReadModel.Label "Read current model")
  (Run.Label "Send to AI")
  (Close.Label "Close")
  (.Layout
   (Grid (Rows 0 1 0 0) (Cols 1)
    Heading
    History
    Prompt
    (Grid (Rows 0) (Cols 1 1 1)
     ReadModel Run Close
    )
   )
  )
 )
)
