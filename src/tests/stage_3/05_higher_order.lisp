; Stage 3: Higher-order functions
; Tests map, filter, reduce patterns

(defun map-list (fn lst)
  (if (null lst)
      nil
      (cons (funcall fn (car lst))
            (map-list fn (cdr lst)))))

(defun square (x) (* x x))

(defun main ()
  (let ((numbers (list 1 2 3 4 5)))
    (format t "Squares: ~a~%" (map-list #'square numbers))))

(main)
