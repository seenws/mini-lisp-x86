; Stage 3: List manipulation
; Tests cons, car, cdr operations

(defun sum-list (lst)
  (if (null lst)
      0
      (+ (car lst) (sum-list (cdr lst)))))

(defun main ()
  (let ((numbers (list 1 2 3 4 5)))
    (format t "Sum: ~d~%" (sum-list numbers))))

(main)
