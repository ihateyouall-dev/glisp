;;; hash.lisp - filling and reading a hash map.
;;;
;;; A small initial table has to grow here, which is amortized, so the numbers
;;; check that the growth is not the dominant cost.

(defun key (i) (string-append "key-" (to-string i)))

(set table (hash))
(dotimes (i 3000)
  (hash-set table (key i) (* i 2)))

(set total 0)
(dotimes (i 3000)
  (set total (+ total (hash-get table (key i)))))

(println (format "{} keys, sum of values {}" (len table) total))
(assert (= (len table) 3000))
(assert (= (hash-get table "key-2999") 5998))
