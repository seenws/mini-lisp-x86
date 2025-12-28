; Stage 2: Recursion
; Tests recursive function calls

(defun factorial (n)
  (if (<= n 1)
      1
      (* n (factorial (- n 1)))))

(factorial 5)
