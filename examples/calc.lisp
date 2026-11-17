;;; calc.lisp - a calculator built on read and eval.
;;;
;;; Each line of input is read as data and then evaluated, which is the point
;;; of read/eval: the expression does not have to be known when the program is
;;; written, only when it runs.
;;;
;;; Run it with: ./build/glisp examples/calc.lisp

(defconst INPUT "(+ 1 2)
(* 3 4)
;; a comment line is skipped below
(let ((x 5)) (* x x))
(abs -7)
(if (> 3 2) 1 0)")

(defun keep-line? (line)
  "True for lines that are neither blank nor a comment."
  (let ((trimmed (string-trim line)))
    (and (> (len trimmed) 0)
         (not (string-starts-with? trimmed ";")))))

(defun evaluate-lines (lines)
  "Evaluates every readable line and returns the results in order."
  (let ((results nil))
    (each (lambda (line)
            (if (keep-line? line)
                (set results (cons (eval (read line)) results))))
          lines)
    (reverse results)))

(set results (evaluate-lines (string-lines INPUT)))
(each (lambda (value) (println value)) results)

(assert (= (len results) 5))
(assert (= (nth results 0) 3))
(assert (= (nth results 1) 12))
(assert (= (nth results 2) 25))
(assert (= (nth results 3) 7))
(assert (= (nth results 4) 1))
(println "calc ok")
