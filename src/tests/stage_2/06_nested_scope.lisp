; Stage 2: Nested scoping
; Tests variable shadowing and closure

(let ((x 1))
  (let ((x 2)
        (y x))
    (list x y)))

(defun make-counter ()
  (let ((count 0))
    (lambda () (setq count (+ count 1)))))
