; Stage 2: Conditional expressions
; Tests if/cond control flow

(if (> x 10)
    (format t "Greater")
    (format t "Less or equal"))

(cond
  ((< x 0) (format t "Negative"))
  ((= x 0) (format t "Zero"))
  (t (format t "Positive")))

