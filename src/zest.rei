let observe:
  (
    ~id: string,
    ~db: Timeseries.t,
    ~main_endpoint: string=?,
    ~router_endpoint: string=?,
    ~path: string,
    ~key: string,
    ~token: string=?,
    ~max_age: int=?, 
    unit
  ) =>
  Lwt.t(unit);