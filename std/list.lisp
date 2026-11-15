;;; list.lisp - List accessors and operations

(defun list (&rest args)
  "Returns ARGS as a list."
  args)

(defun first (x)
  "Returns the first element of X."
  (car x))

(defun last (x)
  "Returns the last element of X. Linear in the list length."
  (nth x (1- (len x))))

(defun caar (x)
  (car (car x)))

(defun cadr (x)
  (car (cdr x)))

(defun cdar (x)
  (cdr (car x)))

(defun cddr (x)
  (cdr (cdr x)))

(defun caddr (x)
  (car (cdr (cdr x))))

(defun reverse (lst)
  "Returns a new list with the elements of LST in reverse order."
  (let ((acc nil))
    (while (not (nil? lst))
      (set acc (cons (car lst) acc))
      (set lst (cdr lst)))
    acc))

(defun append (a b)
  "Returns the concatenation of lists A and B."
  (let ((rev (reverse a))
        (acc b))
    (while (not (nil? rev))
      (set acc (cons (car rev) acc))
      (set rev (cdr rev)))
    acc))

(defun member (item lst)
  "Returns the sublist starting at ITEM, or nil when ITEM is absent."
  (let ((cur lst))
    (while (and (not (nil? cur)) (not (= (car cur) item)))
      (set cur (cdr cur)))
    cur))

(defun take (lst count)
  "Returns the first COUNT elements of LST as a new list."
  (assert (int? count) "COUNT must be integer")
  (let ((acc nil)
        (cur lst)
        (i 0))
    (while (and (< i count) (not (nil? cur)))
      (set acc (cons (car cur) acc))
      (set cur (cdr cur))
      (set i (+ i 1)))
    (reverse acc)))

(defun drop (lst count)
  "Returns LST without its first COUNT elements, sharing the remaining cells."
  (assert (int? count) "COUNT must be integer")
  (let ((cur lst)
        (i 0))
    (while (and (< i count) (not (nil? cur)))
      (set cur (cdr cur))
      (set i (+ i 1)))
    cur))

(defun zip (a b)
  "Returns a list of two-element lists from A and B, stopping at the shorter one."
  (let ((acc nil))
    (while (and (not (nil? a)) (not (nil? b)))
      (set acc (cons (list (car a) (car b)) acc))
      (set a (cdr a))
      (set b (cdr b)))
    (reverse acc)))

(defun flatten (x)
  "Returns a flat list of the atoms in X, descending into nested lists."
  (let ((acc nil)
        (pending (list x)))
    (while (not (nil? pending))
      (let ((cur (car pending)))
        (set pending (cdr pending))
        (if (list? cur)
            (set pending (append cur pending))
            (set acc (cons cur acc)))))
    (reverse acc)))
