;;; calls.lisp - a tail-recursive sum, the cost of one function call.
;;;
;;; Everything a call does shows up here: frame allocation, argument binding,
;;; and the body's own arithmetic.

(defun sum-to (n acc)
  (if (= n 0)
      acc
      (sum-to (- n 1) (+ acc n))))

(set result (sum-to 200000 0))
(println (format "sum 0..200000 = {}" result))
(assert (= result 20000100000))
