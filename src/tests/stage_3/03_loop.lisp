; Stage 3: Looping constructs
; Tests iteration and side effects

(defun count-to-ten ()
  (loop for i from 1 to 10
        do (format t "~d " i))
  (format t "~%"))

(defun main ()
  (count-to-ten))

(main)
