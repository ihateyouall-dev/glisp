;;; lists.lisp - building and walking lists.
;;;
;;; Each recursive step passes the accumulated list, which a function binds to a
;;; copy, so this is the quadratic case by design: it measures the copy, not the
;;; cons.

(defun build (n acc)
  (if (= n 0)
      acc
      (build (- n 1) (cons n acc))))

(set data (build 1500 nil))
(set doubled (map (lambda (x) (* x 2)) data))
(set evens (filter even? doubled))
(println (format "built {} elements, {} doubled, {} even" (len data) (len doubled) (len evens)))
(assert (= (len data) 1500))
(assert (= (car data) 1))
(assert (= (last data) 1500))
(assert (= (len evens) 1500))
