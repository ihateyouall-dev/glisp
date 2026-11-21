;;; core.lisp - Core predicates and boolean operations

;; Popular aliases from other lisps
(defconst setq set)
(defconst set! set)

(defun not (x)
  "Returns t when X is nil, otherwise nil."
  (if x nil t))

(defun zero? (x)
  "Returns t when X is zero."
  (= x 0))

(defun pos? (x)
  "Returns t when X is positive."
  (> x 0))

(defun neg? (x)
  "Returns t when X is negative."
  (< x 0))

(defun identity (x)
  "Returns X unchanged."
  x)

(defun id (x)
  "Returns X unchanged."
  x)

(defmacro and (&rest forms)
  (if forms
      (let ((head (car forms))
            (tail (cdr forms)))
        (if tail
            `(if ,head (and ,@tail) nil)
            `(if ,head t nil)))
      t))

(defmacro or (&rest forms)
  (if forms
      (let ((head (car forms))
            (tail (cdr forms)))
        (if tail
            `(if ,head t (or ,@tail))
            `(if ,head t nil)))
      nil))

(defmacro when (condition &rest body)
  "Evaluate BODY if CONDITION is true."
  `(if ,condition (progn ,@body)))

(defmacro unless (condition &rest body)
  "Evaluate BODY if CONDITION is false."
  `(if ,condition nil (progn ,@body)))

(defun %cond-expand (clauses)
  (if clauses
      (let ((clause (car clauses))
            (rest (cdr clauses)))
        (let ((test (car clause))
              (body (cdr clause)))
          (if (eq test 'else)
              (if body `(progn ,@body) t)
              (if body
                  `(if ,test (progn ,@body) ,(%cond-expand rest))
                  (let ((tmp (gensym "cond")))
                    `(let ((,tmp ,test))
                       (if ,tmp ,tmp ,(%cond-expand rest))))))))
      nil))

(defmacro cond (&rest clauses)
  (%cond-expand clauses))

(defun %case-expand (value clauses)
  (if clauses
      (let ((clause (car clauses))
            (rest (cdr clauses)))
        (let ((key (car clause))
              (body (cdr clause)))
          (if (eq key 'else)
              `(progn ,@body)
              `(if (= ,value ,key)
                   (progn ,@body)
                   ,(%case-expand value rest)))))
      nil))

(defmacro case (value &rest clauses)
  (let ((tmp (gensym "case")))
    `(let ((,tmp ,value))
       ,(%case-expand tmp clauses))))

(defmacro dotimes (spec &rest body)
  (let ((var (car spec))
        (count (car (cdr spec)))
        (result (if (cdr (cdr spec)) (car (cdr (cdr spec))) nil))
        (limit (gensym "limit"))
        (idx (gensym "idx")))
    `(let ((,var 0) (,limit ,count) (,idx 0))
       (assert (and (= (typeof ,limit) int-t) (>= ,limit 0))
               "Dotimes count must be a non-negative integer")
       (while (< ,idx ,limit)
         (set ,var ,idx)
         ,@body
         (set ,idx (+ ,idx 1)))
       ,@(if result (cons result nil) nil))))

;; These are macros rather than functions because they have to update the
;; caller's binding: a function only ever sees a copy of its argument.
(defmacro incf (place &optional (delta 1))
  "Adds DELTA to the variable PLACE and returns the new value."
  `(set ,place (+ ,place ,delta)))

(defmacro decf (place &optional (delta 1))
  "Subtracts DELTA from the variable PLACE and returns the new value."
  `(set ,place (- ,place ,delta)))
