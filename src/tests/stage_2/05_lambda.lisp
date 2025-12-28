; Stage 2: Lambda expressions
; Tests anonymous functions

((lambda (x y) (+ x y)) 10 20)

(let ((add-ten (lambda (n) (+ n 10))))
  (funcall add-ten 5))
