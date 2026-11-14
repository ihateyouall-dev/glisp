;;; Allocation, collection, closure, and I/O exercise used by the macOS leak
;;; check in CI. The script must terminate normally so that `leaks --atExit`
;;; can inspect the process at exit.

(defun make-adder (n)
  "Returns a closure that adds N to its argument."
  (lambda (x) (+ x n)))

(set add-ten (make-adder 10))
(assert (= (add-ten 5) 15))

(set acc nil)
(set i 0)
(while (< i 1000)
  (set acc (cons (cons i (* i 2)) acc))
  (set i (1+ i)))

(assert (= (len acc) 1000))
(assert (= (car (car acc)) 999))

(set acc (reverse acc))
(assert (= (car (car acc)) 0))

(set mapped (map 1+ [1 2 3 4 5]))
(assert (= mapped [2 3 4 5 6]))

(set filtered (filter odd? {1 2 3 4 5 6}))
(assert (= filtered {1 3 5}))

(set total (reduce + mapped 0))
(assert (= total 20))

(set f (open-write "/tmp/glisp-leak-test.txt"))
(write-string "leak check\n" f)
(flush f)
(close f)

(set f (open-read "/tmp/glisp-leak-test.txt"))
(assert (= (read-line f) "leak check"))
(close f)

(pragma gc-collect)
(pragma gc-collect)

(println "leak-check-ok")
