;;; fizzbuzz.lisp - the classic exercise, written as a loop over a range.
;;;
;;; Run it with: ./build/glisp examples/fizzbuzz.lisp

(defun fizzbuzz (n)
  "Returns the fizzbuzz word for N."
  (cond
    ((= (% n 15) 0) "fizzbuzz")
    ((= (% n 3) 0) "fizz")
    ((= (% n 5) 0) "buzz")
    (else (to-string n))))

(println (string-join (map fizzbuzz (range 1 16)) " "))

(assert (= (fizzbuzz 15) "fizzbuzz"))
(assert (= (fizzbuzz 3) "fizz"))
(assert (= (fizzbuzz 5) "buzz"))
(assert (= (fizzbuzz 7) "7"))
(println "fizzbuzz ok")
