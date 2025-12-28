; Stage 3: Quicksort implementation
; Comprehensive test of language features

(defun filter (pred lst)
  (cond ((null lst) nil)
        ((funcall pred (car lst))
         (cons (car lst) (filter pred (cdr lst))))
        (t (filter pred (cdr lst)))))

(defun quicksort (lst)
  (if (null lst)
      nil
      (let* ((pivot (car lst))
             (rest (cdr lst))
             (less (filter (lambda (x) (< x pivot)) rest))
             (greater (filter (lambda (x) (>= x pivot)) rest)))
        (append (quicksort less)
                (list pivot)
                (quicksort greater)))))

(defun main ()
  (let ((unsorted (list 3 7 1 9 2 5 8 4 6)))
    (format t "Unsorted: ~a~%" unsorted)
    (format t "Sorted: ~a~%" (quicksort unsorted))))

(main)
