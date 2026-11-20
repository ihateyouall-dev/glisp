;;; macros.lisp - macros build code instead of computing values.
;;;
;;; A macro receives its arguments as data and returns a form that is then
;;; evaluated, so it can decide what code to produce. Quasiquote (`) builds a
;;; template, unquote (,) drops a value in, and unquote-splicing (,@) splices a
;;; list into the template.
;;;
;;; Run it with: ./build/glisp examples/macros.lisp

(defmacro twice (form)
  "Evaluates FORM two times and returns the second value."
  `(progn ,form ,form))

(defmacro my-when (test &rest body)
  "Evaluates BODY when TEST is true."
  `(if ,test (progn ,@body) nil))

(defmacro with-doubled (name value &rest body)
  "Binds NAME to VALUE doubled while BODY runs."
  `(let ((,name (* 2 ,value)))
     ,@body))

;; A recursive macro: expansion happens until no call is left.
(defmacro countdown (n)
  (if (<= n 0)
      `nil
      `(progn (println ,n) (countdown ,(- n 1)))))

(set counter 0)
(twice (set counter (+ counter 1)))
(println (format "twice ran the form {} times" counter))

(println (format "with-doubled: {}" (with-doubled n 21 (+ n 1))))
(println (format "my-when: {}" (my-when (> 3 2) "yes")))
(countdown 3)

;; macroexpand-1 shows one step of expansion, which is the way to see what a
;; macro actually produced.
(println (macroexpand-1 '(twice (println 1))))
(println (macroexpand-1 '(my-when t 1 2)))

;; The expansions are what the evaluator runs, so a macro has to build a form,
;; not a value: (twice (+ 1 2)) is two additions, not one.
(assert (= counter 2))
(assert (= (with-doubled n 21 (+ n 1)) 43))
(assert (= (my-when (> 3 2) "yes") "yes"))
(assert (nil? (my-when nil "yes")))
(assert (= (twice 5) 5))
(println "macros ok")

;; A macro that binds a helper name would break if the caller happened to have a
;; binding with the same name. gensym produces a name that cannot collide, so
;; the swap below leaves the caller's own tmp alone.
(defmacro swap! (a b)
  (let ((tmp (gensym "swap")))
    `(let ((,tmp ,a))
       (set ,a ,b)
       (set ,b ,tmp))))

(set x 1)
(set y 2)
(set tmp 99)
(swap! x y)
(println (format "after swap: x={} y={} tmp={}" x y tmp))
(assert (= x 2))
(assert (= y 1))
(assert (= tmp 99))

(println "swap ok")
