//! Splits the byte stream from the pool into lines (port of the buffering in
//! `StratumApi::receiveJsonRpcLine`). The socket reads stay in C++: it asks
//! for free space, receives into it and commits the received bytes.

/// Same capacity as the C++ buffer (16 KiB including its NUL terminator).
pub const CAPACITY: usize = 16 * 1024 - 1;

pub struct LineBuffer {
    buf: Box<[u8]>,
    len: usize,
}

impl Default for LineBuffer {
    fn default() -> Self {
        Self {
            buf: vec![0; CAPACITY].into_boxed_slice(),
            len: 0,
        }
    }
}

impl LineBuffer {
    /// Removes and returns the next complete line, without `\n` and an
    /// optional `\r` before it.
    pub fn take_line(&mut self) -> Option<Vec<u8>> {
        let newline = self.buf[..self.len].iter().position(|&b| b == b'\n')?;
        let mut line = self.buf[..newline].to_vec();
        if line.last() == Some(&b'\r') {
            line.pop();
        }
        self.buf.copy_within(newline + 1..self.len, 0);
        self.len -= newline + 1;
        Some(line)
    }

    /// Free space for the next read; empty when the buffer is full.
    pub fn space(&mut self) -> &mut [u8] {
        &mut self.buf[self.len..]
    }

    /// Marks `n` bytes of [`space`](Self::space) as received.
    pub fn commit(&mut self, n: usize) {
        self.len = (self.len + n).min(CAPACITY);
    }

    pub fn clear(&mut self) {
        self.len = 0;
    }
}

mod ffi {
    use super::*;

    #[no_mangle]
    pub extern "C" fn stratum_v1_lines_new() -> *mut LineBuffer {
        Box::into_raw(Box::default())
    }

    /// # Safety
    /// `lines` must come from `stratum_v1_lines_new()` (or be NULL).
    #[no_mangle]
    pub unsafe extern "C" fn stratum_v1_lines_free(lines: *mut LineBuffer) {
        if !lines.is_null() {
            drop(Box::from_raw(lines));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn feed(lb: &mut LineBuffer, data: &[u8]) {
        let space = lb.space();
        space[..data.len()].copy_from_slice(data);
        lb.commit(data.len());
    }

    #[test]
    fn splits_lines_across_reads() {
        let mut lb = LineBuffer::default();
        feed(&mut lb, b"{\"a\":1}\r\n{\"b\"");
        assert_eq!(lb.take_line().as_deref(), Some(&b"{\"a\":1}"[..]));
        assert_eq!(lb.take_line(), None);
        feed(&mut lb, b":2}\n\n");
        assert_eq!(lb.take_line().as_deref(), Some(&b"{\"b\":2}"[..]));
        assert_eq!(lb.take_line().as_deref(), Some(&b""[..]));
        assert_eq!(lb.take_line(), None);
    }

    #[test]
    fn reports_full_buffer() {
        let mut lb = LineBuffer::default();
        let n = lb.space().len();
        assert_eq!(n, CAPACITY);
        lb.commit(n);
        assert!(lb.space().is_empty());
        assert_eq!(lb.take_line(), None);
        lb.clear();
        assert_eq!(lb.space().len(), CAPACITY);
    }
}
