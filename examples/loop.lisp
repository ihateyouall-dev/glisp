;;; loop.lisp - tail calls turn recursion into iteration.
;;;
;;; The sum below needs a hundred thousand steps. Written without a tail call it would
;;; stop at the depth limit; written as it is, the call at the end of the body
;;; reuses the frame, so the loop runs in constant stack.
;;;
;;; Run it with: ./build/glisp examples/loop.lisp

(defun sum-to (n acc)
  "Returns 0 + 1 + ... + N using an accumulator."
  (if (= n 0)
      acc
      (sum-to (- n 1) (+ acc n))))

(defun count-matches (pred coll)
  "Returns how many elements of COLL satisfy PRED."
  (let ((count 0))
    (each (lambda (x) (if (pred x) (set count (+ count 1)))) coll)
    count))

(println (format "sum 0..100000 = {}" (sum-to 100000 0)))
(println (format "even numbers below 10: {}" (count-matches even? (range 0 10))))
(println (format "squares of 1..5: {}" (map (lambda (x) (* x x)) (range 1 6))))

(assert (= (sum-to 100000 0) 5000050000))
(assert (= (sum-to 10 0) 55))
(assert (= (count-matches even? (range 0 10)) 5))
(assert (= (map (lambda (x) (* x x)) [1 2 3]) [1 4 9]))
(println "loop ok")
