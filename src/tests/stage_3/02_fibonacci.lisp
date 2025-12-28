; Stage 3: Fibonacci sequence
; Tests arithmetic, recursion, and code generation

(defun fib (n)
  (if (<= n 1)
      n
      (+ (fib (- n 1))
         (fib (- n 2)))))

(defun main ()
  (format t "fib(10) = ~d~%" (fib 10)))

(main)
